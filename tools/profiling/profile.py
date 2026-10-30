#!/usr/bin/env python3
"""Bounded, external Linux process profiling; Python standard library only."""
import argparse
import datetime as dt
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

SCHEMA = 1
CONFIG_KEYS = {
    'Perf.Enable', 'PerformanceLog.PlayerUpdateSummaryInterval',
    'Diagnostics.Architecture.Enabled', 'Diagnostics.Architecture.IntervalMs',
    'AiPlayerbot.Enabled', 'AiPlayerbot.MinRandomBots', 'AiPlayerbot.MaxRandomBots',
    'AiPlayerbot.botActiveAlone', 'AiPlayerbot.PerfMonEnabled',
    'AiPlayerbot.DisableBotOptimizations', 'AiPlayerbot.DisableActivityPriorities',
    'AiPlayerbot.ForceActiveWhenNearPlayer', 'AiPlayerbot.LimitCombatActivity',
    'AiPlayerbot.DiffWithPlayer', 'AiPlayerbot.DiffEmpty',
    'AiPlayerbot.EnableActionLog', 'AiPlayerbot.MemoryTelemetryInterval',
    'AiPlayerbot.BehaviorTrace',
}


def utc_now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def command(argv):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=15)
    if result.returncode:
        raise RuntimeError(f'{argv[0]} exited {result.returncode}: {result.stderr.strip()[:500]}')
    return result.stdout.strip()


def parse_stat(text):
    """comm may contain spaces and parentheses; fields start after its final ')'."""
    left, right = text.index('('), text.rindex(')')
    fields = text[right + 1:].split()
    return {
        'pid': int(text[:left].strip()), 'name': text[left + 1:right],
        'state': fields[0], 'cpu_ticks': int(fields[11]) + int(fields[12]),
        'start_ticks': int(fields[19]), 'threads': int(fields[17]),
        'minor_faults': int(fields[7]), 'major_faults': int(fields[9]),
    }


def numeric_fields(text, separator=':'):
    result = {}
    for line in text.splitlines():
        key, sep, value = line.partition(separator)
        if sep and re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', key.strip()):
            tokens = value.split()
            if tokens and tokens[0].isdigit():
                result[key.strip()] = int(tokens[0])
    return result


def selected_config(paths):
    """Never copy complete configuration files or connection strings."""
    result = []
    for path in paths:
        values = {}
        for line in Path(path).read_text().splitlines():
            key, sep, value = line.partition('=')
            key = key.strip()
            if sep and key in CONFIG_KEYS:
                value = value.split('#', 1)[0].strip()
                if re.fullmatch(r'-?\d+(?:\.\d+)?', value):
                    values[key] = value
                else:
                    values[key] = 'unrecognised numeric setting'
        result.append({'path': str(path), 'explicit_settings': values})
    return result


class ProcReader:
    def __init__(self, root=Path('/proc'), hz=None):
        self.root = Path(root)
        self.hz = hz if hz is not None else os.sysconf('SC_CLK_TCK')

    def stat(self, pid):
        return parse_stat((self.root / str(pid) / 'stat').read_text())

    def optional(self, path, errors):
        try:
            return path.read_text()
        except OSError as error:
            errors.append(f'{path.name}: {type(error).__name__}')
            return None

    def snapshot(self, pid, expected_start):
        base = self.root / str(pid)
        stat = self.stat(pid)
        if stat['start_ticks'] != expected_start:
            raise RuntimeError('Target PID was reused; capture stopped')
        if stat['state'] in ['Z', 'X', 'x']:
            raise RuntimeError('Target process exited; capture stopped')
        errors = []
        status = numeric_fields((base / 'status').read_text())
        io = self.optional(base / 'io', errors)
        threads = []
        for path in sorted((base / 'task').iterdir()):
            try:
                thread = parse_stat((path / 'stat').read_text())
                threads.append(thread)
            except (FileNotFoundError, ProcessLookupError):
                continue  # A thread can exit during enumeration.
            except PermissionError:
                errors.append(f'thread {path.name}: PermissionError')
        mem = self.optional(self.root / 'meminfo', errors)
        vm = self.optional(self.root / 'vmstat', errors)
        # Check again: never combine measurements from a recycled process ID.
        if self.stat(pid)['start_ticks'] != expected_start:
            raise RuntimeError('Target PID changed while sampling; capture stopped')
        return {
            'monotonic_s': time.monotonic(), 'utc': utc_now(), 'process': stat,
            'memory_kib': {key: status.get(key) for key in
                ['VmRSS', 'VmHWM', 'VmSwap', 'RssAnon', 'RssFile', 'RssShmem']},
            'io_bytes': None if io is None else numeric_fields(io),
            'threads': threads,
            'host_memory_kib': None if mem is None else {key: numeric_fields(mem).get(key)
                for key in ['MemTotal', 'MemAvailable', 'SwapTotal', 'SwapFree']},
            'host_swap_pages': None if vm is None else {key: numeric_fields(vm, ' ').get(key)
                for key in ['pswpin', 'pswpout']},
            'unavailable': errors,
        }

    def rollup(self, pid, expected_start):
        errors = []
        if self.stat(pid)['start_ticks'] != expected_start:
            raise RuntimeError('Target PID was reused before memory snapshot')
        text = self.optional(self.root / str(pid) / 'smaps_rollup', errors)
        values = None if text is None else numeric_fields(text)
        if self.stat(pid)['start_ticks'] != expected_start:
            raise RuntimeError('Target PID was reused during memory snapshot')
        return {'utc': utc_now(), 'memory_kib': values, 'unavailable': errors}


def resolve_target(args):
    if args.pid is not None:
        return args.pid, None
    # Docker top reports host PIDs. State.Pid alone may select an init wrapper.
    metadata = json.loads(command(['docker', 'inspect', '--type', 'container', '--format',
        '[{{json .Id}},{{json .Image}},{{json .State.Running}}]', args.container]))
    if not metadata[2]:
        raise RuntimeError('Container is stopped; this tool does not start it')
    rows = command(['docker', 'top', args.container, '-eo', 'pid,comm']).splitlines()[1:]
    matches = []
    for row in rows:
        fields = row.split()
        if len(fields) == 2 and fields[1] == args.process_name:
            matches.append(int(fields[0]))
    if len(matches) != 1:
        raise RuntimeError(f'Expected exactly one {args.process_name} process; found {len(matches)}')
    cgroup = Path(f'/proc/{matches[0]}/cgroup').read_text()
    if metadata[0] not in cgroup:
        raise RuntimeError('Container PID does not match a local host cgroup; use this tool on the Docker host')
    return matches[0], {'id': metadata[0], 'image_id': metadata[1], 'name': args.container}


def symbols(pid):
    result = {'readelf_available': shutil.which('readelf') is not None}
    try:
        result['executable'] = os.readlink(f'/proc/{pid}/exe')
        if result['readelf_available']:
            text = command(['readelf', '--notes', '--section-headers', f'/proc/{pid}/exe'])
            match = re.search(r'Build ID:\s*(\S+)', text)
            result.update(build_id=match.group(1) if match else None,
                has_debug_info='.debug_info' in text or '.zdebug_info' in text,
                has_debug_link='.gnu_debuglink' in text,
                has_symbol_table='.symtab' in text)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        result['unavailable'] = str(error)
    return result


def preflight(args, reader):
    pid, container = resolve_target(args)
    stat = reader.stat(pid)
    sample = reader.snapshot(pid, stat['start_ticks'])
    errors = []
    paranoid = reader.optional(reader.root / 'sys/kernel/perf_event_paranoid', errors)
    return {
        'schema': SCHEMA, 'utc': utc_now(), 'pid': pid, 'start_ticks': stat['start_ticks'],
        'process_name': stat['name'], 'container': container, 'clock_ticks_per_second': reader.hz,
        'kernel': os.uname().release, 'logical_cpus': os.cpu_count(),
        'perf_available': shutil.which('perf') is not None,
        'perf_permissions': 'Not tested; an explicit --perf capture verifies attachment',
        'perf_event_paranoid': paranoid.strip() if paranoid is not None else None,
        'unavailable': errors,
        'symbols': symbols(pid), 'config': selected_config(args.config),
        'initial_sample': sample,
    }


def perf_command(pid, seconds, output):
    return ['perf', 'record', '-e', 'cpu-clock:u', '-F', '49', '--call-graph',
        'dwarf,8192', '-p', str(pid), '-o', str(output / 'perf.data'), '--', 'sleep', str(seconds)]


def stop_profiler(child):
    if child is not None and child.poll() is None:
        # The profiler owns a separate session containing only perf and its sleep.
        try:
            os.killpg(child.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait(timeout=5)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def capture(args, reader):
    metadata = preflight(args, reader)
    if args.perf and not metadata['perf_available']:
        raise RuntimeError('perf is unavailable; install it separately or omit --perf')
    output = Path(args.output).resolve()
    output.mkdir(parents=True, mode=0o700, exist_ok=False)
    metadata.update(scenario=args.scenario, reported_online_bots=args.bots,
        reported_running_revision=args.revision, workload_notes=args.notes,
        requested_duration_s=args.duration,
        interval_s=args.interval, completed=False, failure=None, perf=None,
        cpu_units='100 percent is one fully occupied logical CPU; may exceed 100',
        note='Reported revision/bot count are operator observations, not inferred from the checkout')
    write_json(output / 'metadata.json', metadata)
    child = None
    perf_log = None
    failure = None
    try:
        pid, start_ticks = metadata['pid'], metadata['start_ticks']
        write_json(output / 'memory-start.json', reader.rollup(pid, start_ticks))
        errors = []
        maps = reader.optional(reader.root / str(pid) / 'maps', errors)
        if maps is not None:
            (output / 'maps.txt').write_text(maps)
        metadata['maps_unavailable'] = errors
        if args.perf:
            if shutil.disk_usage(output).free < 512 * 1024 * 1024:
                raise RuntimeError('Less than 512 MiB free for the optional perf capture')
            seconds = min(args.perf_seconds, args.duration)
            argv = perf_command(pid, seconds, output)
            metadata['perf'] = {'argv': argv, 'requested_duration_s': seconds, 'returncode': None}
            perf_log = (output / 'perf.log').open('w')
            child = subprocess.Popen(argv, stdout=perf_log, stderr=subprocess.STDOUT, start_new_session=True)
        started = time.monotonic()
        with (output / 'samples.jsonl').open('w') as samples:
            deadline = started
            while True:
                snapshot = reader.snapshot(pid, start_ticks)
                samples.write(json.dumps(snapshot, allow_nan=False) + '\n')
                samples.flush()
                if child is not None and child.poll() not in [None, 0]:
                    raise RuntimeError('perf attachment/capture failed; see perf.log')
                if snapshot['monotonic_s'] - started >= args.duration:
                    break
                deadline = min(deadline + args.interval, started + args.duration)
                time.sleep(max(0, deadline - time.monotonic()))
        write_json(output / 'memory-end.json', reader.rollup(pid, start_ticks))
        if child is not None:
            try:
                returncode = child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                raise RuntimeError('perf exceeded its bounded duration') from None
            if returncode != 0:
                raise RuntimeError('perf capture failed; see perf.log')
        metadata['completed'] = True
    except (OSError, RuntimeError, ValueError, KeyboardInterrupt) as error:
        failure = error
        metadata['failure'] = type(error).__name__ + ': ' + str(error)
    finally:
        stop_profiler(child)
        if child is not None:
            metadata['perf']['returncode'] = child.returncode
        if perf_log is not None:
            perf_log.close()
        metadata['finished_utc'] = utc_now()
        write_json(output / 'metadata.json', metadata)
    summary = summarize(output)
    write_json(output / 'summary.json', summary)
    print(json.dumps({'output': str(output), 'completed': metadata['completed'], 'summary': summary}))
    if failure is not None:
        raise RuntimeError(metadata['failure']) from None


def percentile(values, percentage):
    return sorted(values)[max(0, math.ceil(len(values) * percentage / 100) - 1)] if values else None


def summarize(output):
    output = Path(output)
    metadata = json.loads((output / 'metadata.json').read_text())
    if metadata['schema'] != SCHEMA:
        raise ValueError('Unsupported capture schema')
    samples = []
    sample_file = output / 'samples.jsonl'
    if sample_file.exists():
        samples = [json.loads(line) for line in sample_file.read_text().splitlines() if line.strip()]
    cpu, intervals, thread_totals = [], [], {}
    hz = metadata['clock_ticks_per_second']
    for previous, current in zip(samples, samples[1:]):
        elapsed = current['monotonic_s'] - previous['monotonic_s']
        if elapsed <= 0 or current['process']['start_ticks'] != previous['process']['start_ticks']:
            raise ValueError('Inconsistent process identity or sample timestamps')
        ticks = current['process']['cpu_ticks'] - previous['process']['cpu_ticks']
        if ticks < 0:
            raise ValueError('Process CPU counter decreased')
        intervals.append(elapsed)
        cpu.append(100 * ticks / hz / elapsed)
        old = {(t['pid'], t['start_ticks']): t for t in previous['threads']}
        for thread in current['threads']:
            key = thread['pid'], thread['start_ticks']
            # Do not attribute prior lifetime CPU to a new/recycled thread ID.
            if key in old:
                delta = thread['cpu_ticks'] - old[key]['cpu_ticks']
                if delta < 0:
                    raise ValueError('Thread CPU counter decreased')
                row = thread_totals.setdefault(key, {'tid': key[0], 'start_ticks': key[1],
                    'name': thread['name'], 'cpu_seconds_observed': 0})
                row['cpu_seconds_observed'] += delta / hz
    elapsed = sum(intervals)
    rss = [s['memory_kib']['VmRSS'] for s in samples if s['memory_kib']['VmRSS'] is not None]
    swap = [s['memory_kib']['VmSwap'] for s in samples if s['memory_kib']['VmSwap'] is not None]
    io_delta = None
    if len(samples) >= 2 and samples[0]['io_bytes'] is not None and samples[-1]['io_bytes'] is not None:
        io_delta = {key: samples[-1]['io_bytes'].get(key, 0) - samples[0]['io_bytes'].get(key, 0)
            for key in ['read_bytes', 'write_bytes']
            if key in samples[-1]['io_bytes'] and key in samples[0]['io_bytes']}
    return {
        'completed': metadata['completed'], 'failure': metadata['failure'], 'samples': len(samples),
        'observed_duration_s': elapsed,
        'cpu_one_core_percent': {
            'mean': sum(c * t for c, t in zip(cpu, intervals)) / elapsed if elapsed else None,
            'p95_sample': percentile(cpu, 95), 'max_sample': max(cpu) if cpu else None},
        'rss_kib': {'first': rss[0] if rss else None, 'last': rss[-1] if rss else None,
            'max_sample': max(rss) if rss else None},
        'swap_kib_max_sample': max(swap) if swap else None, 'io_bytes_delta': io_delta,
        'busiest_threads': sorted(thread_totals.values(), key=lambda row: row['cpu_seconds_observed'], reverse=True)[:20],
        'limitations': [
            'CPU sample percentiles are not world/map tick or gameplay latency percentiles.',
            'Thread deltas omit work before first sighting and after last sighting; process totals include it.',
            'RSS samples are approximate; memory-start/end.json contain PSS snapshots when permitted.',
            'Host memory/swap includes the database and other processes; process RSS is not container memory.',
            'External profiling has overhead; compare repeated matching runs with and without perf.'],
    }


def bounded_float(low, high):
    def parse(value):
        number = float(value)
        if not math.isfinite(number) or not low <= number <= high:
            raise argparse.ArgumentTypeError(f'Expected a finite number between {low} and {high}')
        return number
    return parse


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError('Expected a positive integer')
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    for action in ['preflight', 'capture']:
        sub = commands.add_parser(action)
        target = sub.add_mutually_exclusive_group(required=True)
        target.add_argument('--pid', type=positive_int, help='Linux host PID')
        target.add_argument('--container', help='Explicit running container name or ID; never starts it')
        sub.add_argument('--process-name', default='mangosd', help='Exact comm to select within a container')
        sub.add_argument('--config', action='append', default=[], help='Read only allowlisted numeric settings')
        if action == 'capture':
            sub.add_argument('--output', required=True, help='New private run directory; existing paths are refused')
            sub.add_argument('--scenario', required=True, choices=['empty', 'idle', 'follow', 'combat', 'loot', 'busy'])
            sub.add_argument('--bots', required=True, type=int, help='Observed online bot count, supplied by operator')
            sub.add_argument('--revision', required=True, help='Verified running binary revision, not the source checkout')
            sub.add_argument('--notes', default='', help='Route/map, party makeup, activity and matching-run details')
            sub.add_argument('--duration', type=bounded_float(1, 600), default=120)
            sub.add_argument('--interval', type=bounded_float(1, 30), default=1)
            sub.add_argument('--perf', action='store_true', help='Attach perf to the target; never launches a server')
            sub.add_argument('--perf-seconds', type=bounded_float(1, 60), default=30)
    sub = commands.add_parser('summarize')
    sub.add_argument('directory')
    args = parser.parse_args(argv)
    try:
        if args.action == 'summarize':
            print(json.dumps(summarize(args.directory), indent=2, allow_nan=False))
            return 0
        if sys.platform != 'linux':
            raise RuntimeError('Capture/preflight run on the Linux host; summarize works on any platform')
        if args.action == 'capture' and args.bots < 0:
            raise ValueError('Bot count cannot be negative')
        reader = ProcReader()
        if args.action == 'preflight':
            print(json.dumps(preflight(args, reader), indent=2, allow_nan=False))
        else:
            capture(args, reader)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'profile: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())

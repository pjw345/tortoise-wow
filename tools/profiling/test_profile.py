"""Evidence accuracy, lifecycle/failure tests, and an isolated Linux smoke capture."""
import argparse
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('turtle_profile', Path(__file__).with_name('profile.py'))
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


def stat_text(pid=42, start=500, cpu=100, name='map ) worker', state='S'):
    # Linux field numbers, independent of the production parser's indexes.
    fields = {3: state, 10: '7', 12: '2', 14: str(cpu), 15: '0', 20: '2', 22: str(start)}
    return f'{pid} ({name}) ' + ' '.join(fields.get(n, '0') for n in range(3, 53))


def sample(timestamp, cpu, rss=1024, threads=None, start=500):
    return {'monotonic_s': timestamp, 'process': {'cpu_ticks': cpu, 'start_ticks': start},
        'memory_kib': {'VmRSS': rss, 'VmSwap': 0},
        'io_bytes': {'read_bytes': cpu * 2, 'write_bytes': cpu * 3},
        'threads': threads or []}


def thread(tid, start, cpu):
    return {'pid': tid, 'start_ticks': start, 'cpu_ticks': cpu, 'name': f'worker {tid}'}


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def proc(self):
        process = self.root / '42'
        (process / 'task/42').mkdir(parents=True)
        (process / 'stat').write_text(stat_text())
        (process / 'task/42/stat').write_text(stat_text())
        (process / 'status').write_text('VmRSS: 2048 kB\nVmSwap: 128 kB\n')
        (process / 'io').write_text('read_bytes: 50\nwrite_bytes: 100\n')
        (self.root / 'meminfo').write_text('MemTotal: 8000 kB\nMemAvailable: 2000 kB\n')
        (self.root / 'vmstat').write_text('pswpin 3\npswpout 4\n')
        return profile.ProcReader(self.root, hz=100)

    def bundle(self, samples, completed=True):
        profile.write_json(self.root / 'metadata.json', {
            'schema': 1, 'clock_ticks_per_second': 100,
            'completed': completed, 'failure': None if completed else 'interrupted'})
        (self.root / 'samples.jsonl').write_text(''.join(json.dumps(s) + '\n' for s in samples))

    def capture_args(self, perf=False):
        return argparse.Namespace(output=str(self.root / 'run'), scenario='idle', bots=20,
            revision='verified-test-revision', notes='fixture only', duration=1,
            interval=1, perf=perf, perf_seconds=1)

    def metadata(self):
        return {'schema': 1, 'pid': 42, 'start_ticks': 500,
            'perf_available': True, 'clock_ticks_per_second': 100}

    def test_stat_comm_with_spaces_and_parentheses_and_cpu_fields(self):
        result = profile.parse_stat(stat_text(cpu=235))
        self.assertEqual((result['pid'], result['name'], result['cpu_ticks'], result['start_ticks']),
            (42, 'map ) worker', 235, 500))
        self.assertEqual((result['minor_faults'], result['major_faults']), (7, 2))

    def test_snapshot_units_and_disappearing_thread(self):
        reader = self.proc()
        (self.root / '42/task/99').mkdir()  # Thread exited before its stat was read.
        result = reader.snapshot(42, 500)
        self.assertEqual(result['memory_kib']['VmRSS'], 2048)
        self.assertEqual(result['host_swap_pages'], {'pswpin': 3, 'pswpout': 4})
        self.assertEqual(result['io_bytes']['read_bytes'], 50)
        self.assertEqual(len(result['threads']), 1)

    def test_pid_reuse_before_and_during_snapshot(self):
        reader = self.proc()
        with self.assertRaisesRegex(RuntimeError, 'reused'):
            reader.snapshot(42, 499)
        first = reader.stat(42)
        changed = dict(first, start_ticks=999)
        with patch.object(reader, 'stat', side_effect=[first, changed]):
            with self.assertRaisesRegex(RuntimeError, 'changed while'):
                reader.snapshot(42, 500)

    def test_zombie_is_not_a_successful_target(self):
        reader = self.proc()
        (self.root / '42/stat').write_text(stat_text(state='Z'))
        with self.assertRaisesRegex(RuntimeError, 'exited'):
            reader.snapshot(42, 500)

    def test_unavailable_pss_and_io_are_not_reported_as_zero(self):
        reader = self.proc()
        (self.root / '42/io').unlink()
        result = reader.snapshot(42, 500)
        self.assertIsNone(result['io_bytes'])
        self.assertIn('io: FileNotFoundError', result['unavailable'])
        rollup = reader.rollup(42, 500)
        self.assertIsNone(rollup['memory_kib'])

    def test_rollup_mapping_header_is_not_a_memory_metric(self):
        reader = self.proc()
        (self.root / '42/smaps_rollup').write_text(
            '001000-002000 ---p 00000000 00:00 0 [rollup]\nRss: 100 kB\nPss: 60 kB\n')
        self.assertEqual(reader.rollup(42, 500)['memory_kib'], {'Rss': 100, 'Pss': 60})

    def test_weighted_cpu_and_recycled_thread_id(self):
        self.bundle([
            sample(0, 0, threads=[thread(1, 10, 0), thread(2, 20, 500)]),
            sample(1, 200, threads=[thread(1, 10, 100), thread(2, 99, 999)]),
            sample(4, 300, rss=2048, threads=[thread(1, 10, 150), thread(2, 99, 1049)]),
        ])
        result = profile.summarize(self.root)
        self.assertAlmostEqual(result['cpu_one_core_percent']['mean'], 75)
        self.assertAlmostEqual(result['cpu_one_core_percent']['max_sample'], 200)
        observed = {row['start_ticks']: row['cpu_seconds_observed'] for row in result['busiest_threads']}
        self.assertEqual(observed, {10: 1.5, 99: 0.5})
        self.assertEqual(result['io_bytes_delta'], {'read_bytes': 600, 'write_bytes': 900})
        self.assertEqual(result['rss_kib']['max_sample'], 2048)

    def test_invalid_identity_or_counter_history_is_rejected(self):
        histories = [
            [sample(0, 5), sample(1, 6, start=999)],
            [sample(1, 5), sample(1, 6)],
            [sample(0, 5), sample(1, 4)],
        ]
        for history in histories:
            self.bundle(history)
            with self.assertRaises(ValueError):
                profile.summarize(self.root)

    def test_empty_failed_capture_stays_failed_with_unknown_metrics(self):
        self.bundle([], completed=False)
        result = profile.summarize(self.root)
        self.assertFalse(result['completed'])
        self.assertIsNone(result['cpu_one_core_percent']['mean'])
        self.assertIsNone(result['rss_kib']['max_sample'])

    def test_config_capture_excludes_credentials_and_non_numeric_values(self):
        path = self.root / 'server.conf'
        path.write_text('WorldDatabaseInfo = password-secret\nAiPlayerbot.botActiveAlone = 10 # comment\n'
            'AiPlayerbot.PerfMonEnabled = password-secret\n# AiPlayerbot.BehaviorTrace = 1\n')
        result = profile.selected_config([path])
        text = json.dumps(result)
        self.assertNotIn('password-secret', text)
        self.assertNotIn('WorldDatabaseInfo', text)
        self.assertEqual(result[0]['explicit_settings']['AiPlayerbot.botActiveAlone'], '10')
        self.assertNotIn('AiPlayerbot.BehaviorTrace', result[0]['explicit_settings'])

    def test_container_selects_server_instead_of_init_and_rejects_ambiguity(self):
        args = argparse.Namespace(pid=None, container='world', process_name='mangosd')
        with patch.object(profile, 'command', side_effect=['["container-id","image-id",true]',
                'PID COMMAND\n1 tini\n42 mangosd']), \
                patch.object(Path, 'read_text', return_value='0::/system.slice/docker-container-id.scope'):
            self.assertEqual(profile.resolve_target(args)[0], 42)
        with patch.object(profile, 'command', side_effect=['["container-id","image-id",true]',
                'PID COMMAND\n42 mangosd']), patch.object(Path, 'read_text', return_value='0::/different-host'):
            with self.assertRaisesRegex(RuntimeError, 'local host cgroup'):
                profile.resolve_target(args)
        for rows in ['PID COMMAND\n1 tini', 'PID COMMAND\n42 mangosd\n43 mangosd']:
            with patch.object(profile, 'command', side_effect=['["id","image",true]', rows]):
                with self.assertRaisesRegex(RuntimeError, 'exactly one'):
                    profile.resolve_target(args)
        with patch.object(profile, 'command', return_value='["id","image",false]') as run:
            with self.assertRaisesRegex(RuntimeError, 'stopped'):
                profile.resolve_target(args)
            self.assertEqual(run.call_count, 1)

    def test_target_exit_saves_partial_capture_and_failure(self):
        reader = Mock()
        reader.root = self.root
        reader.rollup.return_value = {'memory_kib': None, 'unavailable': []}
        reader.optional.return_value = None
        reader.snapshot.side_effect = [sample(0, 0), FileNotFoundError('target exited')]
        with patch.object(profile, 'preflight', return_value=self.metadata()), \
                patch.object(profile.time, 'monotonic', return_value=0), \
                patch.object(profile.time, 'sleep'), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, 'target exited'):
                profile.capture(self.capture_args(), reader)
        metadata = json.loads((self.root / 'run/metadata.json').read_text())
        summary = json.loads((self.root / 'run/summary.json').read_text())
        self.assertFalse(metadata['completed'])
        self.assertEqual(summary['samples'], 1)

    def test_perf_permission_failure_cannot_be_reported_as_success(self):
        reader = Mock(root=self.root)
        reader.rollup.return_value = {}
        reader.optional.return_value = None
        reader.snapshot.return_value = sample(0, 0)
        child = Mock(returncode=255)
        child.poll.return_value = 255
        with patch.object(profile, 'preflight', return_value=self.metadata()), \
                patch.object(profile.subprocess, 'Popen', return_value=child), \
                patch.object(profile.shutil, 'disk_usage', return_value=Mock(free=1024**3)), \
                contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, 'perf attachment'):
                profile.capture(self.capture_args(perf=True), reader)
        metadata = json.loads((self.root / 'run/metadata.json').read_text())
        self.assertFalse(metadata['completed'])
        self.assertEqual(metadata['perf']['returncode'], 255)

    def test_existing_output_cannot_be_overwritten(self):
        output = self.root / 'run'
        output.mkdir()
        sentinel = output / 'original.txt'
        sentinel.write_text('keep')
        with patch.object(profile, 'preflight', return_value=self.metadata()):
            with self.assertRaises(FileExistsError):
                profile.capture(self.capture_args(), Mock())
        self.assertEqual(sentinel.read_text(), 'keep')

    def test_non_finite_or_unbounded_durations_rejected(self):
        for value in ['nan', 'inf', '-inf', '0', '601']:
            with self.assertRaises(argparse.ArgumentTypeError):
                profile.bounded_float(1, 600)(value)

    @unittest.skipUnless(sys.platform == 'linux', 'Requires Linux /proc; runs in CI')
    def test_live_linux_process_capture_does_not_stop_target(self):
        child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'])
        try:
            args = self.capture_args()
            args.pid, args.container, args.config = child.pid, None, []
            with contextlib.redirect_stdout(io.StringIO()):
                profile.capture(args, profile.ProcReader())
            self.assertIsNone(child.poll(), 'The collector must not terminate its target')
            result = profile.summarize(self.root / 'run')
            self.assertTrue(result['completed'])
            self.assertGreaterEqual(result['samples'], 2)
            self.assertGreater(result['rss_kib']['max_sample'], 0)
        finally:
            child.terminate()
            child.wait(timeout=5)

    @unittest.skipUnless(sys.platform == 'linux', 'Requires POSIX process groups; runs in CI')
    def test_live_profiler_cleanup_leaves_target_running(self):
        target = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'])
        profilers = []
        original_popen = subprocess.Popen
        try:
            args = self.capture_args(perf=True)
            args.pid, args.container, args.config = target.pid, None, []
            reader = profile.ProcReader()
            metadata = profile.preflight(args, reader)
            metadata['perf_available'] = True
            first = reader.snapshot(target.pid, metadata['start_ticks'])

            def launch(*positional, **keywords):
                child = original_popen(*positional, **keywords)
                profilers.append(child)
                return child

            with patch.object(profile, 'preflight', return_value=metadata), \
                    patch.object(profile, 'perf_command', return_value=[sys.executable, '-c', 'import time; time.sleep(30)']), \
                    patch.object(profile.subprocess, 'Popen', side_effect=launch), \
                    patch.object(reader, 'snapshot', side_effect=[first, RuntimeError('stop fixture capture')]), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, 'stop fixture capture'):
                    profile.capture(args, reader)
            self.assertIsNone(target.poll())
            self.assertEqual(len(profilers), 1)
            self.assertIsNotNone(profilers[0].poll())
        finally:
            for child in profilers:
                if child.poll() is None:
                    profile.stop_profiler(child)
            target.terminate()
            target.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()

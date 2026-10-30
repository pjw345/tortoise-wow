# Server profiling workflow

Start with an unchanged running build and repeated, matching workloads. This
workflow captures evidence before choosing optimisation patches. It does not
start a VM, container, database or game service, install tools, change configuration,
query game databases, or launch a server build. Preparation and fixture tests work
with the game VM off. Actual collection runs on the **Linux Docker host inside
the VM**, after the user starts the VM and game services separately.

## 1. Record the workload and check readiness

Python 3.9+ is the only requirement for process sampling. `perf` is optional;
`readelf` adds executable build-ID and symbol-section checks. Run as the same
user as the target, or an account already permitted to inspect it. Do not change
`perf_event_paranoid`, capabilities or container privileges just to get past a
failed preflight. Report a missing tool/access requirement separately.

Identify the existing world container with `docker ps`; use its exact name below.
The collector selects the one `mangosd` process reported by `docker top`, rather
than assuming the container's PID 1 is the server. It verifies that the PID belongs
to a matching local cgroup; a remote Docker context is not a local process target.
Alternatively, specify a verified Linux host PID with `--pid`.

```sh
python3 tools/profiling/profile.py preflight --container WORLD_CONTAINER
```

Add `--config /path/to/mangosd.conf --config /path/to/aiplayerbot.conf` when the
actual active files have been located. Only explicitly present, allowlisted
numeric performance/activity settings are captured; absent values are not guessed.
Do not pass source templates as if they were the active runtime configuration.
The tool never copies full configs, process command lines, environment variables,
or complete Docker inspection output.

Record the running revision from the deployment receipt/startup version and
verify it against the selected image. A checkout commit is not proof of the
running binary. For Docker, this read-only query helps identify the image:

```sh
docker inspect --format '{{.Image}}' WORLD_CONTAINER
```

Also record VM vCPU/RAM allocation, other host/VM workloads, client/addon version,
observed online humans/bots and activity mix, party/classes/formation, map/route,
and whether the world is loading or settled. `--bots` and `--revision` are explicitly
labelled operator observations in the output. Use `--notes` for the other details.

## 2. Capture the baseline without perf first

Replace the capitalised arguments with the observations above. Use a new output
directory for every run; existing directories are refused. Run from the repository
root so the default example output location is ignored by Git.

```sh
python3 tools/profiling/profile.py capture --container WORLD_CONTAINER \
  --scenario follow --bots OBSERVED_BOT_COUNT --revision VERIFIED_RUNNING_REVISION \
  --duration 120 --interval 1 --output profiling-results/follow-01 \
  --notes 'Map/route, party, activity mix, settled population and other host load'
```

There is no automatic population change. Start with the current population and
your normal party. Run each available scenario three times, keeping the route,
duration, warm-up, population and settings the same:

| Scenario | Workload |
| --- | --- |
| `empty` | Optional separate zero-player/bot baseline; requires a separately arranged test population |
| `idle` | Settled normal population, party stationary |
| `follow` | Repeatable travel route, including doors/corners and terrain changes |
| `combat` | Same location and similar pulls, including healing/spells |
| `loot` | Comparable corpse looting and gathering, including return to follow |
| `busy` | Same populated town/location after loading settles |

Loading and steady state are separate workloads. Do not combine their figures.
After these are repeatable, consider 20/50/100/200/500 bot stages as resources and
separate user-directed population changes permit. Observe memory pressure and
gameplay at each stage rather than jumping straight to 500.

## 3. Capture one short CPU profile of an expensive scenario

Repeat the matching workload with a new output path and add
`--perf --perf-seconds 30`. The collector attaches to the existing process with:

```text
perf record -e cpu-clock:u -F 49 --call-graph dwarf,8192 -p HOST_PID -o RUN/perf.data -- sleep 30
```

The software user-CPU event avoids requiring virtual hardware cycle counters.
The initial rate is 49 samples/second per monitored busy thread; DWARF stack
dumping has overhead and may truncate deep stacks. The CPU profile covers the
first 30 seconds, while the process samples cover the full requested duration.
Captures are bounded to 600 seconds, and optional perf to 60 seconds. The duration
bound is not a file-size quota; reserve disk space. Attachment failure marks the
run incomplete and retains `perf.log`, rather than silently producing a successful
CPU profile. Interrupting the collector stops only its profiler child/session.

Compare matching runs with and without perf before interpreting small differences.
The data measures executing user code. Kernel time, blocked threads, lock waits,
database waits and world/map latency require separate interpretation.

```sh
python3 tools/profiling/profile.py summarize profiling-results/follow-01
perf report --stdio -i profiling-results/follow-perf-01/perf.data
perf buildid-list -i profiling-results/follow-perf-01/perf.data
```

Preserve the exact executable, shared libraries and corresponding debug symbols
from the captured image before replacing it. Preflight reports ELF build ID,
symbol table, embedded debug info and separate-debug-link presence when `readelf`
can inspect them. A missing embedded debug section does not rule out separate
debug files. Resolve unknown frames before attributing costs to functions.
For container paths, analysis on the same host can use
`perf report --stdio --symfs /proc/HOST_PID/root -i RUN/perf.data` while that exact
container/process still exists; copied symbol trees must preserve matching paths
and build IDs for later analysis.

Keep the initial executable/build flags unchanged. The repository's
`ENABLE_PROFILING` option is Windows Optick integration and explicitly changes
async behaviour; it is not the switch for this Linux baseline. Do not enable
Tracy, sanitizers, PGO generation, change allocators/LTO or switch build type
as part of the first comparison. If symbols are insufficient, plan a separate
optimised symbol-bearing build with the same settings and validate it first.

## 4. Read the results correctly

Each run contains `metadata.json`, `samples.jsonl`, `summary.json`, mappings,
start/end `smaps_rollup` snapshots where permitted, and optional perf data/logs.
CPU means use actual elapsed sample time. **100% means one logical CPU**; a
multithreaded server can exceed 100%. These figures are not divided by the VM's
CPU count or container quota. Inspect per-thread costs before concluding that
spare overall CPU means the world thread has capacity.

RSS and PSS are KiB, process I/O is bytes, and host swap counters are pages.
RSS sampling is approximate; start/end PSS accounts proportionally for shared
mappings. Unavailable values remain null with a reason, not zero. RSS growth alone
does not demonstrate a leak: repeat routes after warm-up and distinguish retained
map/cache capacity from unbounded growth. Whole-container memory includes charges
that are not process RSS; host memory includes MariaDB and all other processes.

Thread IDs include their start times so recycled IDs are not combined. Short-lived
thread work between samples can be absent from thread rankings; process CPU still
accounts for it. Samples are read sequentially, not as an atomic kernel snapshot.
Target exit/reuse or failed optional perf leaves a non-successful partial bundle.

CPU sample percentiles are **not world-tick or gameplay latency percentiles**.
Use existing `perf.log` architecture summaries only for their documented fields:
histogram values are bucket bounds, nested elapsed timings are inclusive, and
slow-only warnings cannot establish the full latency distribution. Mark latency
acceptance pending if a full distribution is unavailable; do not invent p95/p99.
Correlate the run's UTC window and scenario with the existing logs, without
enabling every verbose trace. The collector does not copy these logs automatically.

Inventory the active probes using [TURTLE_DIAGNOSTICS.md](../../doc/TURTLE_DIAGNOSTICS.md)
and audit findings [A5/A6](../../docs/CORE_COMPATIBILITY_AUDIT_2026-09-05.md).
`ExecutionWatch` clocks/atomics and tracked-allocation mutexes remain separate from
summary logging; their costs are candidates for measurement, not established
bottlenecks. The bot `PerformanceMonitor` has shared mutable maps and a commented-out
data lock: review its map/worker threading before enabling it broadly. Never remove
ownership, lifetime or watchdog safeguards merely to improve a graph.

Measure MariaDB independently with a verified host PID and comparable run window
if it is expensive. This tool does not query SQL or enable database logging.
Heap allocation profiling and wait-stack analysis are later targeted steps;
RSS samples and on-CPU stacks cannot identify allocation ownership or blocked time.

Keep raw bundles local: perf DWARF samples contain stack bytes, and mappings/logs
may include private paths or gameplay data. Share a reviewed summary or filtered
stacks when possible. `profiling-results/` is ignored; do not commit raw captures.

## 5. Turn evidence into one focused optimisation PR

Rank hotspots by measured share of CPU/allocation cost, frequency and gameplay
impact. Trace the native callers, state/lifetime/thread ownership and supported
variants before changing them. Reuse the existing caches/schedulers/APIs where
their contracts fit. Compare three matching before/after runs and check following,
combat/healing, loot, quests and recovery. Record exact build/image, workload,
diagnostic settings, resource differences and gameplay acceptance limits.

This first workflow changes no C++ behaviour. It cannot claim CPU/RAM savings
until real captures and gameplay checks have been performed.

## Verification without the game VM

```sh
python3 -m unittest discover -s tools/profiling -p test_profile.py -v
```

Fixtures cover CPU units/weighting, process/thread reuse, disappearing threads,
missing access, stopped/ambiguous/remote containers, incomplete captures, perf
failure, credential exclusion and output preservation. CI also captures a disposable
sleeping Python process on Linux and verifies the target remains running. That is
collector validation, not TurtleWoW profiling or gameplay acceptance.

References: [Linux procfs](https://docs.kernel.org/filesystems/proc.html),
[upstream perf record](https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf-record.txt),
[perf tutorial/build IDs/access](https://perfwiki.github.io/main/tutorial/),
[Docker top](https://docs.docker.com/reference/cli/docker/container/top/).

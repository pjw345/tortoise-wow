# Party battle capture

This opt-in recorder observes native combat. It does not change targeting,
healing, resource costs, movement, threat, cooldowns, items or bot strategies.
It is separate from the old sampled per-bot action log: one sampled file per
bot cannot establish complete party damage/healing totals or battle boundaries.

## Boundaries

- A successful human `CMSG_ATTACKSWING` command or accepted client-started hostile
  spell opens a battle. Ground/caster-centred hostile spells are supported.
  Failed input, automatic retaliation, bot attacks and procs do not open one.
- Further attacks/target changes reuse that party's battle. All current group
  members are included; ownership attributes pets/guardians to their player.
- Native combat transitions update scalar member states. Player updates close
  the log once every member is known idle. The leader dying does not end it while
  companions are fighting. A pending hostile opener cannot close before launch.
  A cancelled opener that never enters combat is labelled accordingly.
- Off-map members supply their own observations on their map; their state is
  not read across map ownership. Offline members are known idle. Unknown loaded
  members keep the battle open. Roster changes, leaving the world, shutdown,
  configuration reload and capture limits produce explicit incomplete endings.
- An attempted cross-map unit-state read is replaced with a scope-gap event and
  an incomplete capture. Crashes/SIGKILL can leave no end marker; the reader
  recognises that. Up to the last second of buffered data can be lost on a crash.

The requested window begins with the player's attack. Aggro or damage before
that window is not reconstructed. Initial party combat flags reveal if members
were already fighting when capture opened.

## Enable and storage

Build the logging revision first. Set these in the **effective mangosd config**:

```ini
BattleLog.Enabled = 1
BattleLog.PlayerGuid = 0
```

`PlayerGuid=0` permits any human initiator. To isolate a character, use its low
character GUID. This is configuration, never a hardcoded character exception.
The shipped default is disabled. Loading/reloading native configuration applies
the switch and closes older sessions as incomplete. Capture writes no chat,
accounts, database rows or client AddOn data.

Files are `LogsDir/battles/battle-<UTC epoch microseconds>-<leader GUID>-<serial>.jsonl`.
An empty LogsDir uses `../logs/battles`. Default limits:

| Setting | Default | Meaning |
|---|---:|---|
| `BattleLog.MaxFileMiB` | 8 | One capture's byte budget |
| `BattleLog.MaxEvents` | 50000 | Unsampled event budget |
| `BattleLog.MaxFiles` | 100 | Retained owned capture files |
| `BattleLog.MaxSessions` | 8 | Concurrent parties |
| `BattleLog.MaxMinutes` | 30 | Maximum observed session duration |

Caps end capture with `complete=false`; they never silently sample events.
Retention removes only closed regular files with this facility's exact numeric
filename pattern. Symlinks, open files and unrelated files are preserved.
With defaults, capture storage is bounded to roughly 800 MiB. Reports generated
later are separate files and are not pruned by the runtime recorder.

Writes use stdio buffering and flush about once per second and at end. All file
and recorder state is mutex-protected; game objects are copied on their owning
map and never retained. There are no new background scans or worker threads.
Disabled hooks return immediately. Enabled logging adds serialization/I/O and
500 ms player state samples; gameplay overhead still needs measurement in game.

## What is captured

| Native observation | Captured facts |
|---|---|
| Attack/combat transitions | Actor/target, native combat flags, alive state, selected/victim GUIDs, map/instance, positions, fleeing flags |
| `Spell::prepare`, successful start, launch, result and finish | Stable cast ID, spell ID/name, item ID/name, target, cast time, triggered state, native heal classification, resource pools |
| `Spell::TakePower` | Actual before/after resource values and percentages; no charge is inferred from an attempt |
| Aura upkeep | Actual periodic native resource/health cost; no original cast ID is invented |
| `Unit::SetPower` | Actual clamped pool changes, including gains, refunds and drains, without guessing causes |
| `WorldObject::EnergizeBySpell` | Explicit spell/potion power gain and recipient |
| `Spell::TakeCastItem` | Actual consumed stack count after native destruction, separately from item casts |
| `Unit::DealDamage` | Unsampled post-mitigation requested damage and actual HP loss, melee/spell/DoT identity, absorb/resist/outcome where supplied |
| `WorldObject::DealHeal` and native periodic heal | Actual requested/effective healing and overheal; full-health periodic ticks are included |
| Miss/aura paths | Native spell miss reason, aura apply/remove and original caster GUID |
| Threat manager | Recorded native threat changes/current victim; no synthetic threat formula |
| Motion master / bot chase policy | Chase/point requests, bot chase intent, blocked fleeing pursuit and sampled position changes |
| Bot engine / spell probes | Executed/failed/impossible action labels, native eligibility failure codes, active combat strategies, healer/ranged classification and relevant profession ranks |

GUIDs and cast IDs are JSON strings to preserve 64-bit identity. Timestamps use
monotonic milliseconds plus a UTC epoch timestamp at battle start. Native rage
is stored in tenths: 100 raw units means 10 displayed rage. Mana/rage percentages
are written explicitly. `resource_spent` is the charged amount; `resource_change`
also includes that same change and must **not** be added to spell costs again.

`cast_result`/`cast_check` contain this fork's enum (`SPELL_CAST_OK=255`).
`cast_finish` uses 0 for successful finish and 1 for unsuccessful finish;
interrupting wand fire normally is not automatically a failure of the AI.
The companion code-name map is generated from this fork's SpellDefines.h.

Some native periodic/central healing calls do not receive a Spell instance.
Their actual actor/spell/healing totals are logged, but their original cast ID
is left unknown. Item casts, stack consumption and multi-charge item depletion
are distinct: stack consumption is recorded here; multi-charge use without
stack destruction is not represented as a destroyed item.

## Summaries and timeline

```sh
python3 tools/summarize-battle-log.py /path/to/logs/battles
# Or one capture, with a chosen report directory:
python3 tools/summarize-battle-log.py /path/to/battle-....jsonl --output /path/to/reports
```

The tool writes `.summary.md`, `.summary.json` and `.events.csv` per input.
The CSV contains time, attack/spell/target, effective damage/heal, actual cost,
pool percentages, item consumption and result codes. The report compares party
damage/DPS, DoTs, wand damage, mana/rage costs and trajectories, effective healing
versus overheal, self versus other-member healing, Paladin versus Priest output,
items, spell eligibility failures, overlapping heal starts, actual threat victim
changes, enemy first contacts, and movement involving runners. Bot roles are
observed from runtime strategies, not inferred from class or saved DB settings.

Incomplete/corrupt/truncated input stays labelled incomplete. The report does
not call an overlapping heal wasteful without actual overheal evidence, does
not treat every wand sample above 50% mana as a bug, and does not claim the first
actor near an add caused its pull. Chase requests, sampled direction and later
contact are evidence for investigation, not proof of causation. External actors'
damage to an enemy outside any recorded party interaction is not a raid-wide
damage meter. Direct raw power-field writes can bypass SetPower logging.

## Validation

`tests/party-battle-log` compiles the actual scalar recorder and validates its
output with the actual report tool. Coverage includes concurrent parties,
leader death, whole-party end, cancelled/instant openers, unknown members,
roster/logout/shutdown, caps, safe retention, escaped names, I/O failure,
unsampled totals, real cost accounting, rage conversion, overheal and damaged
input. Native wiring checks accompany these tests; they are not a substitute
for a full game build or a real captured battle.

First gameplay acceptance: one ordinary fight, then a runner/add fight, then a
leader death while companions continue. Check costs against visible bars/item
stacks, and compare Priest/Paladin healing and Mage spell/wand activity. Do not
mark the combat AI fixed based on diagnostic build or fixture success.

Native assistance runs are recorded separately from the ordinary fleeing unit flag. MoveSeekAssistance can run without UNIT_STAT_FLEEING; runner observations include both assistance movement phases. Native flee-for-assistance, scheduled assistance and call-for-help requests appear in the report timeline. Requests do not prove that helpers responded or which party member caused an add.

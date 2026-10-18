# Turtle core: native systems and change contracts

Source baseline: `mantech-turtle`, `b2d5a8549194f7ffa38e324dcb7a82ccc0ba2132`, reviewed 2026-09-05. This guide is a navigational and ownership reference, not a substitute for reading the current implementation. Line numbers below describe this baseline and will drift.

Start with the [compatibility audit](CORE_COMPATIBILITY_AUDIT_2026-09-05.md), [map coverage](core-audit/COVERAGE.md), and [diagnostic inventory](../doc/TURTLE_DIAGNOSTICS.md). `AGENTS.md` requires using native mechanisms and checking their full contract before making changes.

## How to investigate the next bug

1. Record the exact build, config, client version, entity entry/GUID, map/instance, reproduction and expected behavior. A local source tree is not proof of the running binary.
2. Find the public entry point: opcode, AI hook, spell effect, gossip/event script, DB loader or maintenance callback. Find its callers and the authoritative state it changes.
3. Identify the selected implementation, not just a filename. Check database binding, build inclusion, registration and native fallback. Check optional modules and runtime Lua separately.
4. Trace eligibility checks, caster/target roles, ownership, state transitions, notifications, persistence and failure cleanup. Identify which thread owns the object when each step runs.
5. Compare another working consumer of that same native API. Use CMaNGOS/upstream as references, not as proof of equivalent Turtle semantics.
6. Correct the demonstrated cause in the responsible layer. Do not put an isolated fix in an outer handler when the generic implementation already supports the behavior. Conversely, do not rewrite a shared spell effect to compensate for an invalid caller.
7. Test supported variants, rejected/repeated requests, transitions and persistence; include other consumers of any changed shared function. Record what was not exercised.
8. Update this guide, the finding ledger and regression tests when contracts change. Audit snapshots are evidence at a timestamp, not an evergreen content database.

Useful searches: `rg -n 'SymbolName' src modules tests`, literal `script_name` in the audit JSON, and `git diff <verified-base> -- <path>`. Do not search only dungeon folders: `generic_spell_ai` is registered in the game library.

## Execution and ownership map

Bot startup provisioning (`RandomPlayerbotFactory::CreateRandomBots`) must keep
account-creation futures separate from character-save futures. `get()` consumes
a future; waiting on it again throws `std::future_error`. Drain and clear both
bounded eight-task windows before advancing phases. Keep native `SaveToDB`,
cache registration with the session attached, and subsequent player/session
cleanup in that order. `BotCreationLifecycleTest` executes the production loops
with real futures and mock account/player services; it is not a realm startup
or database-persistence test.

Playerbot travel-search futures remain owned by the AI context until result
consumption or AI destruction. Their snapshot inputs do not remove their reads
of global travel destinations and terrain. `PlayerbotWorldScript::OnShutdown`
snapshots every attached AI from the native player registry (including headless
and self-bot sessions), releases the registry lock, then calls
`PlayerbotAI::WaitForTravelSearch` before World tears down sessions/maps.
The hook drains even after bots have been disabled by a config reload.
`PlayerbotAI` destruction also waits before deleting engines/context. Waiting
preserves results and stored worker exceptions and does not instantiate unused
values. Ordinary reset remains nonblocking; do not reuse a normal reset to join
shutdown work. Run the focused native-future regression with
`cmake -S tests/playerbot-shutdown -B build/shutdown-tests`, build it, then CTest.

| Boundary | Current source entry points | Contract to preserve |
| --- | --- | --- |
| World lifecycle | [World.cpp](../src/game/World.cpp), `World::Update` at 2731 | Global services, transports, session/result processing and map orchestration are separate phases. World-thread maintenance must not mutate a map concurrently with its owner. |
| Map jobs | [MapManager.cpp](../src/game/Maps/MapManager.cpp), `Update` at 336; [Map.cpp](../src/game/Maps/Map.cpp), `DoUpdate` at 1528 | Selected maps get joined owner jobs; transfers/unload occur across the ownership barriers. Serial/parallel choices are alternatives, not permission to execute both. |
| Object discovery and update | `Map::UpdateDiscoveredCells` at 802; [Creature.cpp](../src/game/Objects/Creature.cpp), `Update` at 716 | Discovery collects/deduplicates candidates; the owner runs native object logic. Players/cameras/corpses have separate handling. A second collector is not a second combat engine. |
| Completion and instance state | `Map::CompleteUpdate` at 1673 | Instances complete inside their update; continents use a later manager phase. Retain `UpdateScriptedEvents`, `ScriptsProcess`, optional Eluna, `i_data->Update`, weather and grid lifecycle. |
| Player and bot simulation | `Map::UpdatePlayers` at 1274, `UpdatePlayerAI` at 1396; [PlayerbotScripts.cpp](../modules/mod-playerbots/src/playerbot/PlayerbotScripts.cpp) | Core player state, module bookkeeping, individual AI and synthetic session work are distinct. The idle batch hands off the whole map's AI and waits; do not fan out mutating bots on one map independently. |
| Motion | [Unit.cpp](../src/game/Objects/Unit.cpp), [MotionMaster.cpp](../src/game/Movement/MotionMaster.cpp), `Map::UpdateActiveObjects`/motion work | Native motion and deferred `UpdateAsync` are distinct stages. Preserve queue-or-inline exclusivity, pending-set cleanup, map transitions and generator lifetime. |
| Packet ownership | [WorldSession.cpp](../src/game/WorldSession.cpp), `ProcessPackets` at 480; opcode registration/filter definitions | A handler must execute in the context its opcode permits. Extra queue-drain checkpoints must consume packets, not replay them. Synthetic bot packets retain handlers but intentionally have different socket admission. |
| Persistence | [Database sources](../src/shared/Database), native entity save methods | SQL worker execution and application of results are different ownership stages. Priority queues can reorder work across priorities; callbacks and object references must survive cancellation/shutdown safely. |
| Maintenance | [RandomPlayerbotMgr.cpp](../modules/mod-playerbots/src/playerbot/RandomPlayerbotMgr.cpp), auction module | Population counts include pending work; resumable plans need identity/generation checks. A cooperative budget cannot interrupt a single expensive operation. Preserve native final teleport/auction operations. |

There is no source evidence in these inspected paths of two complete simulation engines. That does **not** mean the architecture port is behavior-neutral, or that every shared-state race is excluded. See findings A1–A6 in the audit.

## Creature, boss and trash AI: actual selection

The important chain is:

`creature` spawn definition / dynamic summon → `creature_template` → `Creature::AIM_Initialize` → `FactorySelector::selectAI` → native `Creature::Update` → selected `AI()->UpdateAI` and lifecycle hooks.

- [CreatureAISelector.cpp](../src/game/AI/CreatureAISelector.cpp), `selectAI` at 37, asks the script manager first for eligible ordinary creatures/non-controlled pets. Possession, controlled pets, charm, totems and guards have special selection rules. Named AI, permit selection and fallback follow.
- [ScriptMgr.cpp](../src/game/ScriptMgr.cpp), `GetCreatureAI` at 1767, supports legacy registered scripts, typed script registries, global creature hooks and optional Eluna. An empty or stale `ai_name` alone does not establish which implementation actually runs.
- [ScriptLoader.cpp](../src/scripts/ScriptLoader.cpp) invokes `AddSC_*`; [scripts CMake](../src/scripts/CMakeLists.txt) and [game CMake](../src/game/CMakeLists.txt) determine source inclusion. File presence and successful compilation do not prove registration was invoked.
- [CreatureAI.cpp](../src/game/AI/CreatureAI.cpp) loads `creature_spells` through `SetSpellsList`; its spell-list update and cast helpers are native mechanisms. [CreatureEventAI.cpp](../src/game/AI/CreatureEventAI.cpp) separately loads `creature_ai_events` and can still run its spell list and melee with no event rows.
- `creature_ai_scripts` stores command scripts referenced by events; it is **not** the table of EventAI event rows. Mixing those schemas leads to incorrect audits and fixes.
- [GenericSpellAI.cpp](../src/game/AI/GenericSpellAI.cpp), registration at 154 and initialization at 390, derives generic behavior from template spell slots. Its presence outside `src/scripts` resolved 52 apparent missing bindings in the first scanner pass.
- [ScriptedAI](../src/game/AI/ScriptedAI.h) and [ScriptedInstance](../src/game/AI/ScriptedInstance.h) provide native combat/instance helpers. Reuse those instead of separate hand-built targeting, spell, door or respawn systems.

### Architecture implications for every encounter

[BackgroundWorldScheduling.h](../src/shared/BackgroundWorldScheduling.h) returns no distant-creature interval for non-continents. `DescribeBackgroundCreature` in `Map.cpp:770` additionally protects script IDs, non-ordinary selected AI types, zone scripts, world-boss rank, escorts, controlled units, combat, relevant auras/events/casting, active objects and transport passengers.

Therefore dungeon/raid actors are outside this particular continent-only throttle. They still depend on shared discovery, map ownership, movement/protocol, spell clocks, DB callbacks and visibility. World-boss protection applies to the boss once selected; nearby helpers, trigger objects, grid activation and custom global hooks still need tracing. Do not interpret the guard as proof that every encounter dependency is protected.

No special conversion is required in each boss file to use the new map scheduling: it runs underneath the native AI. Conversely, adding an extra boss-update loop to "enable the new tech" would risk duplicate mechanics.

## Instances, doors, summons and persistence

- `map_template.script_name` selects the instance implementation. [InstanceData](../src/game/Maps/InstanceData.h) defines lifecycle hooks; [ScriptedInstance.cpp](../src/game/AI/ScriptedInstance.cpp) implements helpers including `DoUseDoorOrButton` at 11.
- Trace `OnCreatureCreate`/`OnObjectCreate`, stored GUIDs, `SetData`/`GetData`/`GetData64`, encounter states, reset/evade/death, save/load and world-object recreation. Correctly opening a door once is insufficient if it remains closed after reload or opens on a failed encounter.
- Default examples: [Blackwing Lair instance](../src/scripts/dungeons/blackwing_lair/instance_blackwing_lair.cpp), [Deadmines instance](../src/scripts/dungeons/deadmines/instance_deadmines.cpp). Custom examples: [Lower Karazhan](../src/scripts/dungeons/lower_karazhan_halls/instance_lower_karazhan_halls.cpp) registers multiple trash AIs as well as the instance; [Solnius](../src/scripts/dungeons/emerald_sanctum/boss_solnius.cpp) connects gossip, instance lookup and boss behavior.
- Summoned bosses/adds may have no ordinary spawn row. Search creature entry constants, summon spells, `SummonCreature`, event commands and summon callbacks. Check cleanup on wipe, phase transitions and grid/map unload.
- Blank `map_template.script_name` is not automatically a bug: some content relies on creature/GO/EventAI/native mechanisms. Identify the intended state owner before adding an instance controller.
- Portals may be visual gameobjects plus separate area-trigger/teleport data. A missing visual object's script binding warrants investigation, not an automatic extra teleport handler.

## Other native entry points and change boundaries

This is a navigation index. The targeted audit did not execute every behavior in the following systems.

| System | Start here | Related consumers / checks before changing |
| --- | --- | --- |
| Casts, melee, damage and death | [SpellHandler.cpp](../src/game/Handlers/SpellHandler.cpp) `HandleCastSpellOpcode:325`; [Spell.cpp](../src/game/Spells/Spell.cpp) `prepare:3507`, `cast:3775`, `CheckCast:5561`; [Unit.cpp](../src/game/Objects/Unit.cpp) `DealDamage:756`, `Kill:1190` | Players, creatures, pets, triggered spells, threat, immunity, procs, auras, rewards and script hooks share these paths. A scheduler must not replay elapsed time as duplicate effects. |
| Spell effects and auras | [SpellEffects.cpp](../src/game/Spells/SpellEffects.cpp), [SpellAuras.cpp](../src/game/Spells/SpellAuras.cpp), [UnitAuraProcHandler.cpp](../src/game/UnitAuraProcHandler.cpp) | Dispatch by actual effect/type. Preserve caster vs target, trigger flags, resources, duration, death/removal and proc ordering. |
| Trainers | [NPCHandler.cpp](../src/game/Handlers/NPCHandler.cpp) `HandleTrainerBuySpellOpcode:285`; [Player.cpp](../src/game/Objects/Player.cpp) `LearnSpell:4670`, `HasSpell:5383`; spell effects 2254/3152 | Player training and pet training are different supported services. The pet path requires a player caster, native eligibility/training-point checks, pet persistence and owner notification. Verify successful learning before payment; failure must not later finish as a free delayed purchase. |
| Loot and inventory | [LootMgr.cpp](../src/game/LootMgr.cpp) `LootTemplate::Process:1324`; [LootHandler.cpp](../src/game/Handlers/LootHandler.cpp) `HandleAutostoreLootItemOpcode:41`; `Player::SendLoot:9451` | Template/reference/group/condition selection is separate from allowed-player checks and inventory storage. Include group distribution, quest loot, pickpocket/skinning, full bags, release/retry and persistence. Do not replace the native eligibility/store sequence. |

Player-specific quest loot is not contained in `Loot::items`. Turtle appends the
eligible player's `m_playerQuestItems` after the shared slots in `LootView`.
Adapters must enumerate `GetMaxSlotInLootFor(playerGuid)` and resolve each slot
through `LootItemInSlot(slot, playerGuid, ...)`; direct indexing of the shared
vector silently omits quest items. Creature eligibility should use
`Player::IsAllowedToLoot`, and the native autostore handler remains authoritative
for storage, quest counters, notifications and failure handling.

A corpse may expose ordinary loot and skinning simultaneously. In that state,
`LootObject::skillId == SKILL_NONE` denotes the ordinary-loot phase: open and
release it before attempting skinning. A failed gathering cast must use a
bounded retry rather than selecting the same corpse again every AI tick.

Playerbot corpse discovery is periodic, so a corpse that remains server-lootable
with only policy-excluded items (for example cloth on a bot configured not to
loot cloth) must be remembered per bot for a bounded interval after inspection.
Do not mark it globally consumed or bypass `Player::IsAllowedToLoot`. Native
open/store failures use the shorter retry interval; a completed inspection uses
`LootTargetInspectDelay`. Optional per-bot diagnostics go to the bounded
`playerbot-loot.log`, not player chat or the main server stream.

Selected loot is no longer present in the available-loot queue. RPG and travel
movement must therefore check both the queue and the current `loot target`, or
the two movement actions can alternately replace one another. A successful
loot approach sets its normal movement wait so the engine does not relaunch the
same path every decision tick. Dynamic `GO_FLAG_LOCKED` state remains
authoritative over a chest template's key requirement: only spawned, loot-bearing
chests whose locked flag has actually been cleared may bypass a missing key.

Ordinary mining and herbalism nodes are game-object spell targets. Their
gathering actions must call the `PlayerbotAI::CastSpell` game-object overload
with the selected node explicitly. Passing the bot as a unit target relies on
an effect-slot heuristic to recover `loot target`; a cast can then report
success without ever opening the node. This core's `setGOTarget` only records
the object pointer and GUID, so both the Playerbot check and cast paths must
also add `TARGET_FLAG_GAMEOBJECT` to the spell target mask. Omitting that bit
allows the gathering animation to play while `EffectOpenLock` receives no node
and produces no loot response. Creature gathering (skinning and creature
mining/herbalism) remains on the separate unit-target path.

Starting a gathering cast is not proof that the node opened. Mining and
herbalism attempts must use a bounded retry lifecycle: once `SpellStart`
accepts the asynchronous cast, clear the current `loot target`, keep the node
in `available loot`, and defer its selection for `LootTargetRetryDelay`. A
successful cast still delivers its loot response to `StoreLootAction`, which
removes the node; a normal orange-skill failure produces no response and the
same queued node becomes eligible again after the delay. This is important for
manually queued `u go` targets, which cannot rely on a periodic gathering scan
to rediscover them. Leaving the node selected makes the normal loot trigger
start it again every AI update, producing an apparently endless gathering
animation.

`Engine::ListenAndExecute` applies the completed action's duration after
`Execute` returns. `OpenLootAction` must therefore copy the asynchronous
spell's duration into the action for game-object opening and creature
gathering casts. Relying only on `PlayerbotAI::WaitForSpellCast` allows the
default action delay to replace that wait; the next ordinary follow update
then interrupts mining, herbalism, skinning or a cast-opened container before
the native spell effect runs.

The same explicit game-object target contract applies to ordinary locked
containers such as food or drink barrels. The generic opening path must pass
the selected game object to `PlayerbotAI::CastSpell`; targeting the bot can play
the interaction animation without giving `EffectOpenLock` an object to loot.

`wait for attack` uses its dedicated `WaitForAttackDistance` (three yards by
default), does not inherit spell, hostile-attack or flee distance, and only
creates space when already inside that configured radius. Candidate points are
screened against the complete no-LOS hostile set and the least movement is
chosen; guard range still bounds repositioning. A pending resurrection request
outranks automatic release, self-resurrection, spirit-healer and repop actions.

The action engine evaluates `isUseful()` before `isPossible()`. A useless
action does not enqueue its alternatives; an impossible action does. Missing,
unresolved or unlearned spell capabilities must therefore fail `isPossible()`,
not `isUseful()`, so the native action graph can fall back from greater heal to
heal and lesser heal. `SpellCastUsefulValue` deliberately allows unresolved
spells for this reason. Target/vehicle/current-cast usefulness checks remain
native; capability rejection still occurs before range and actual casting.
Mount and the generic Paladin blessing actions remain dynamic and retain their
native spell selection. No class-specific heal fallback or priority override is
introduced. Focused native-method tests are in `tests/playerbot-spell-fallback`.

Playerbot `skill` chat commands search `SkillLineEntry::name`, whose vanilla
DBC storage has `MAX_DBC_LOCALE` (eight) entries. `MAX_LOCALE` also includes
the non-vanilla `ruRU` session locale and must never bound that array. Clamp a
requester's preferred locale to the DBC range, scan only valid DBC slots, and
null-check localized names both while matching and while formatting replies.
An invalid or unavailable requester session falls back to `LOCALE_enUS`; it
must not turn an informational chat command into a world-server crash.

| Quests, rewards and XP | [QuestHandler.cpp](../src/game/Handlers/QuestHandler.cpp) `HandleQuestgiverCompleteQuest:588`; `Player::RewardQuest:15269`; [QuestDef.cpp](../src/game/QuestDef.cpp) | Acceptance, objectives, item/money requirements, repeatability, reputation, XP rate modifiers, reward scripts and persistence. A quest relation alone is not proof that objectives work. |
| Trade | [TradeHandler.cpp](../src/game/Handlers/TradeHandler.cpp) `HandleAcceptTradeOpcode:293` | Both accept states, item ownership, bag space, enchant spell targets, money, cancellation and saves. Native code saves both players; that alone is not proof of one cross-player atomic transaction. |
| Chat, WHO and commands | [ChatHandler.cpp](../src/game/Handlers/ChatHandler.cpp) `HandleMessagechatOpcode:176`; [MiscHandler.cpp](../src/game/Handlers/MiscHandler.cpp) `HandleWhoOpcode:271`; [Chat sources](../src/game/Chat) | Client request parsing, filters, result counts/wire fields, security levels, channel membership and visibility. Do not special-case one displayed name or confuse account authority with character level. |
| Travel | [TaxiHandler.cpp](../src/game/Handlers/TaxiHandler.cpp) `HandleActivateTaxiOpcode:203`; [MotionMaster.cpp](../src/game/Movement/MotionMaster.cpp); [TransportMgr.cpp](../src/game/Transports/TransportMgr.cpp) `Update:538`; `Player::TeleportTo:2670` | Taxi flight is not moving-transport simulation. Preserve route data, client/server templates, embark/disembark offsets, transfer ACKs, native relocation, visibility and generator lifetime. Client WDB cache and server DBC/data are distinct. |
| Gameobjects and interaction | [GameObject.cpp](../src/game/Objects/GameObject.cpp) `Use:1477`; ScriptMgr gossip/GO hooks | GO type determines native behavior. Check door/button state, spell activation, use conditions, area triggers, event callbacks and cooldowns before attaching a new script. |
| PvP/group systems | [Battleground sources](../src/game/Battlegrounds), [OutdoorPvP sources](../src/game/OutdoorPvP), [Group.cpp](../src/game/Group/Group.cpp), [ThreatManager.cpp](../src/game/Threat/ThreatManager.cpp) | Team/faction and controlled-unit rules, match lifecycle, objective scripts, group state and threat ownership. PvP is not certified by a PvE test. |

### Trainer lesson to retain

Knowing that a service touches `SPELL_EFFECT_LEARN_SPELL` is only the start. The sibling `SPELL_EFFECT_LEARN_PET_SPELL` uses different state and side effects. The original direct player-learning approach did not cover it. At this baseline the handler reuses the native pet spell path, and a handler regression test covers success/failure/repeat variants. The test uses mocks and does not prove the entire live spell engine. Future changes must inspect sibling effect types and other callers before narrowing a generic service.

## Evidence maintenance

- September 6 bot dispatch: `MovementAction::DispatchMovement` must choose one
  native movement path. Direct/free-flying/single-point requests use MovePoint;
  generated multi-point requests use MovePath after hazard avoidance, with no
  stale point generator underneath. Preserve the first route vertex when
  MoveSplineInit replaces vertex zero with the live position, and pass walking
  mode through Turtle's explicit walk argument. Empty requests do not interrupt
  existing motion. BotMovementDispatchTest executes the real dispatcher,
  MovePath and point initialize/update bodies with deterministic unit/spline
  services. It covers double launches, options, walking, short/empty paths,
  hazard-point retention and point speed reinitialization, not full live trips.

- [Bot technology integration](BOT_TECH_INTEGRATION_2026-09-05.md) adapts the
  CMaNGOS bounded retry engine after native eligibility, excluding combat and
  human-directed activity. Do not move retry admission ahead of prerequisites.
  The shared bot UseTaxi helper validates both Turtle endpoints and discovers
  an unknown source only through a matching interactable flight master; it
  never bypasses native anticheat. MinimalMove must retain failed flight legs.
  Dungeon/avoid-list behavior uses the existing strategy/trigger/action/value
  registries and the existing MoveAwayFromCreature path implementation.
  RPG crowd selection tallies eligible nearby bot targets once per selection;
  do not restore the old >=200-neighbor exemption or per-candidate scan.
  MoveToRpgTargetAction consumes the corrected nav coordinates, rejects stale/
  unreachable targets through native values, and only pauses creature patrols.
  ClosestCorrectPoint must preserve its input on query failure (also used by
  corpse recovery). BotRpgMovementTest covers these native boundary fragments.
  The temporary BehaviorTrace hook reads owner-local state and uses the existing
  core performance log with bounded, rotating GUID samples. It must never run
  target-selection triggers, change masters or enable global verbose logging.
  Controls, limits and removal sites are in TURTLE_DIAGNOSTICS.md.
  A Unit owns a MoveSpline before its first path is initialized: check native
  `Initialized()` before reading Duration()/length-dependent data. Trace callers
  must handle absent/fresh/cleared paths without changing the movement API's
  contract. BotTraceSnapshotTest covers the full enabled snapshot body with
  checked spline storage; limiter-only tests cannot establish snapshot safety.
  The September 6 taxi-specific extension reports requested path/from/to and
  probes the nearest flagged flight master within 20 yards only after trace
  admission. It visits both native object containers, including unavailable
  NPCs, without loading grids, and asks CanInteractWithNPC for its optional
  rejection explanation. The predicate's original checks, order and bool
  result remain unchanged; the probe never supplies a new NPC or changes
  eligibility for gameplay. An activation logs route IDs without probing its
  post-taxi state. NpcInteractionTraceTest exercises the native predicate and
  formatter with deterministic map/DBC/reputation services, not a live flight.

- Selected fork adaptations are recorded in
  [SELECTED_FORK_INTEGRATION_2026-09-05.md](SELECTED_FORK_INTEGRATION_2026-09-05.md).
  Null-owner PathInfo calls return NOPATH, not partial paths; native AB discovery
  retains range/eligibility/capture checks; bot broadcasts honor their global gate.
  PetIsDeadValue caches only its database fallback per value instance, not live
  pet state, and Reset/live-pet observations invalidate that fallback. Do not
  replace this with a global cache or a blanket slow cadence for pet reactions.
  ForkIntegrationTest exercises these boundary fragments, not a live realm.

- Native bot travel searches own `std::async` futures. Resetting/replacing an
  unfinished future can join the worker and block the map owner. Full reset
  expires the travel target, retaining unfinished ownership; all three request
  variants reject replacement until ready. Only PREPARE results are eligible
  for adoption. `GetPartitions` releases its existing five-worker permit on
  exception as well as success. Do not detach workers, skip map joins or add a
  second travel engine. `TravelFutureLifecycleTest` covers the native fragments;
  bot destruction still joins outstanding work and is not a bounded cancellation.

- The playerbot node graph persists links, point geometry and derived costs in
  `ai_playerbot_travelnode*`. Runtime loading now recomputes walk distance and
  water exposure from those stored points while retaining creature-risk data,
  and restores the original/upstream 3,600-divisor taxi route preference. This
  avoids a destructive graph/account reset when derived costs are stale. A*
  retains one native graph and applies a stable, bounded per-party preference
  to comparable edges so large populations do not all select one corridor.
  Destination/point ordering is also seeded by party, purpose and coarse
  position rather than wall-clock timing. Short water crossings retain native
  swim timing; sustained swims receive a bounded safety cost so roads, taxis
  and transports win when available without making a required swim impossible.
  `TravelRoutePolicyTest` covers time units, determinism and the preference
  bound; live validation still requires observing route distribution, taxi
  completion and ordinary player travel at the configured population.

- September 6 flight-ID contract: persisted `flightPath` objects are identifiers
  in the currently loaded native TaxiPath DBC, not stable across client/data
  layouts. The compat `sTaxiNodesStore` reads ObjectMgr's DB-backed taxi nodes;
  `sTaxiPathStore` and path geometry come from `DataDir/dbc`. The SQL `taxipath`
  mirror can be empty and is not the runtime authority. On a complete cached
  graph, `generateAll()` now calls the existing `generateTaxiPaths()` before
  coverage warming. Partial/full generation already calls it and must not call
  it twice. Refresh both IDs and geometry through `setPathTo`, retaining native
  cost/eligibility rules. Do not dirty `hasToSave` solely for this startup pass,
  rewrite all cached geometry in SQL, reset bots, or bypass NPC source checks.
  Native point arrays can contain null holes: generation skips incomplete data
  with a startup count. It does not remove arbitrary cached/custom links when
  native data is missing. `BotTaxiCacheRefreshTest` executes these production
  boundaries; see the bot integration ledger for the all-270 live-data audit.

- Custom aura types 227–230 are native non-immediate modifiers. Registration
  must cover both `AuraHandler` and `AuraProcHandler` and the `TOTAL_AURAS`
  bound. Actual arithmetic belongs in rage, skill cast time, periodic damage
  done and chain damage taken, using existing aura lists/multiplier helpers.
  Do not substitute attacker spell-ID checks for recipient damage reductions.
  Tests: `NativeCustomAuraTest`; exact affected data and limits: audit ledger.

- The [audit correction ledger](AUDIT_FIXES_2026-09-05.md) supersedes resolved
  baseline findings. In particular, use the live native spell map rather than
  adding another capability cache, retain AI elapsed time across admission
  deferrals, and never write item progress into packed creature/GO quest slots.
- Turtle's `quest_cast_objective` defines player-target spell objectives.
  `World::SetInitialWorldSettings` loads those after quests/player cache;
  `ObjectMgr::LoadQuestSpellCastObjectives` deliberately creates synthetic
  objective IDs. A missing creature-template join is not sufficient evidence
  of a broken objective for these quests.

- [audit-core-contracts.ps1](../tools/audit-core-contracts.ps1) regenerates lexical source indexes and content joins. Without `-RefreshDatabase` it only reads the saved snapshot. Explicit refresh takes the existing local query adapter and issues read-only SELECTs against `tw_world`; it requires network access and valid credentials outside the report.
- The script is an investigator's helper, not a C++ parser. It does not resolve preprocessor branches, dynamic registration, runtime Lua, all summon dependencies, expected encounter design, or live availability. CMake hints for modules/shared are unknown rather than assumed enabled.
- The snapshot contains NPC/content information, not player accounts, characters or credentials. DB reads were sequential, not a consistent-transaction snapshot.
- Existing architecture tests are under [tests/architecture](../tests/architecture). `ContentHookContract.cmake` checks source wiring by lexical/regex assertions; passing it does not establish exactly-once execution or all boss mechanics.
- Diagnostic controls/removal belong in the existing [diagnostic inventory](../doc/TURTLE_DIAGNOSTICS.md), not scattered permanent logs. Disabled summary logging does not necessarily remove timers/atomics.
- Record source revision and fresh evidence on every significant update. Repository documentation makes the knowledge reusable; it does not make an assistant infallible or remove the need to reopen current source.

Playerbot speed diagnostics use the existing opt-in action logs; see
[the diagnostic ledger](../doc/TURTLE_DIAGNOSTICS.md#playerbot-speed-diagnostics-2026-10-09)
for event interpretation, overhead and removal controls. Optional reason
arguments on native speed APIs only identify diagnostic callers; packet/ACK,
aura and generator propagation behavior is preserved.

Native speed ordering correction (2026-10-10): a direct `SetSpeedRateReal`
assignment supersedes outstanding requests of the same movement type by
advancing the existing per-type movement counter. Matching late ACKs are
consumed before being rejected as superseded, preventing stale speed, position
and flags from overwriting newer server state. Native timeout/bulk resolution
also skips those requests. Other movement types and flag/teleport ordering are
unchanged; this applies to players, controlled units and bots through the
existing Unit lifecycle. No aura removal, forced normal speed or bot-only
recovery is introduced. Focused coverage executes native setters, queue
resolution, ACK matching and the handler's supersession guard.


### Playerbot build revision dependency

`PlayerbotAIConfig.h` is part of `botpch.h`. Keep `SystemConfig.h` (and its
generated `revision.h`) private to `PlayerbotAIConfig.cpp`: the default filename
is declared `extern` in the header and defined once in the implementation, with
the existing Windows/Unix values and mutable `std::string` type. This prevents
every source SHA change invalidating the entire bot PCH. Explicit revision users
still include SystemConfig directly and rebuild normally. The Ubuntu packaging
incremental regression exercises PCH and private revision-header dependencies;
full native compilation remains required for compatibility validation.

## Creature health scaling and GM list SQL (2026-10-11)

`Creature::SelectLevel` clamps a scaled integer health of zero to one only
when the template has a positive maximum health. Intentionally zero-health
templates remain zero. The native April Fools critter override and initial
health percentage still apply afterwards. Upstream provenance: `0695ec09`.
The focused `tests/upstream-health-sql` harness extracts the actual production
health block, including creation/max/current-health assignment, and tests rate,
level interpolation, zero-template, spawn percentage and seasonal variants.
It does not certify runtime death/respawn or pet behavior.

`HandleGMListFullCommand` quotes `rank` in both SELECT and WHERE. This preserves
the existing positive-rank filter and supports MySQL reserved identifiers;
the broader upstream quoting changes already exist here. The focused SQL test
extracts the actual query and executes it on a disposable MySQL 8.4 account
table, checking zero, negative and positive ranks. No production schema or
account data is needed or changed.

### Active follow-generator inspection (2026-10-11)

Playerbot `ServerFacade::GetChaseTarget`, angle and offset inspect the active
native chase/follow generator for both players and creatures. A combat victim
is not the movement target. Unrelated or absent generators return no target
and zero geometry; do not inspect a suspended generator beneath point movement.
`TargetedMovementGeneratorMedium` exposes its existing follower reference and
geometry without changing their ownership, native pathfinding or async update.

Follow action admission, its update trigger and movement dispatch use the same
`IsFollowing` comparison. The requested formation distance includes the target
radius; the stored native offset does not. Differences within 0.01 yards and
0.01 radians (including angle wraparound) retain the route; changed targets,
formations or movement types issue a new native follow request. Failed path
recovery still belongs to the native follow generator. The focused regression
suite in `tests/playerbot-follow` covers these boundaries with deterministic
motion services; it does not certify Stormwind navigation mesh geometry or
live gameplay. Loot retry loops remain a separate investigation.

### Hostiles near loot for grouped followers (2026-10-11)

`LootObject::IsApproachSafe` applies the configured `LootHostileDistance` to
grouped followers, including those with active human masters. Admission,
selection and movement recheck the same live destination through the native
`GetAllHostileNPCNonPetUnitsAroundWO` query. That query filters living hostile
NPCs from the bot's perspective and excludes players and pets. Neutral/friendly
units and dead corpses do not block loot. A newly unsafe selected target is
deferred with the existing retry lifecycle and cleared before native movement.
Ungrouped bots, group leaders and an explicitly zero safety radius retain their
previous policy. This checks the destination area; it is not proof that every
path to a safe destination is free of aggro or that navigation geometry is valid.

### Player-controlled companion combat focus

`AiPlayerbot.AssistPlayerMaster` defaults to enabled for grouped companions with
an active real player master on the same map/instance, outside PvP. The native
master attack victim (not selection or a raid icon) is the allowed damage target.
PossibleAttackTargetsValue and DPS/tank selectors share that target; AttackAction
and unit/coordinate/pet spell dispatch recheck it. Area spells remain available
against the focus target, including caster-centred and ground-targeted casts;
they do not permit selecting an attacking add instead. Positive spells and
interrupts remain available, including healing. Pure healers do not gain an
automatic melee assist. Existing attack delays still apply when enabled.

UpdatePlayerMasterAssist clears stale target values/auto-attacks and interrupts
disallowed in-progress casts using native APIs; pets attacking a different enemy
are stopped and returned to follow. Bots warn their master privately when a live
enemy attacks them, bounded to one attempt per ten seconds and the existing
no-repeat/silent rules. Random, solo, bot-led and PvP behaviour is retained;
setting the configuration to 0 restores autonomous player-companion targeting.
Projectile spells already launched, existing damage-over-time effects and
hostiles pulled by movement/healing are not undone by this targeting policy.

## Playerbot aggro warning timing

UpdatePlayerMasterAssist waits for two seconds of continuously observed native attackers before warning the human master. Losing all attackers or leaving the assist scope resets this delay. Existing ten-second reminders, targeting, healing and pet behaviour are preserved; timing uses wrap-safe native millisecond differences. The focused tests/playerbot-master-focus fixture executes the production method, including transient aggro, repeat timing and timer wrap.
## Stationary waypoint arrival

Native waypoint Update handles stopped, finalized splines as arrival only when the creature is within the spline displacement tolerance (0.01 yards) of its current node. This allows a route beginning at spawn to advance after MoveSplineInit rejects a zero-length movement. Interrupted movement elsewhere retains the 30-second retry; immobility, distraction, waypoint waits and arrival scripts retain their existing paths. tests/waypoint-arrival extracts the production Update method for focused boundary checks; live escort completion still needs validation.


### Consumable completion and group profession commands (2026-10-15)

Successful native food/drink use registers the resources being recovered with
PlayerbotAI. UpdateConsumableRecovery runs during the serialized bot update,
before the action delay gates engine work. At exact resource completion it uses
SetStandState(STAND), whose native standing-interrupt semantics remove the drink
and food effects, and releases only the consumable wait. Simultaneous eating
must finish health recovery too. Explicit wait/stay and a seated human master
remain authoritative; casts are not interrupted. Reset, expired wait, combat,
death, standing and world/teleport transitions invalidate the tracked recovery.
The normal item and item-cheat paths register only after accepted use/casting.
This corrects the fixed 24-second normal item wait without changing regeneration,
item costs, healing priorities, recovery start thresholds or unrelated waits.

UseGameObject decodes the chest through native LootObject before selecting it.
Mining/herbalism requirements the bot lacks (or has insufficient skill for) are
silent skips, without changing its selected loot or attempting open loot. Other
chests, lockpicking, eligible gatherers, missing-tool/open errors and existing
quest-chest behavior retain their native paths and failure reports.

### Bounded resource-node pings (2026-10-15)

RevealGatheringItemAction and repeat validation share CanRevealGatheringItem:
current spawn/loot state, range and native lock/profession requirements remain
required. One selected node produces four native Ping packets, three seconds
apart. A bounded GUID/map/instance/group-ID state lives on the bot; no retained
object/group pointers, new threads, world scans or delayed raw-pointer callbacks
are added. Each repeat resolves the node on the serialized AI owner. One repeat
at most is sent per update; overdue updates never burst all missed pings.
A new reveal does not replace an unfinished sequence. Combat, death, teleport,
map/instance/group change or disabling reveal cancels repeats. Node availability,
range and skill are rechecked when a repeat is due. This adds no gathering,
movement or chat actions, and is controlled by the existing reveal strategy.

The extracted native recovery, group object command and ping eligibility/timer
methods are covered in tests/playerbot-recovery-loot. Full compilation and live
playtesting remain separate validation stages.

## Dead human leader combat focus (2026-10-16)

Human-master combat focus requires a living master. A dead/ghost master exits focus mode, preserving native attacker selection, damage permission and pet reaction restoration; revival re-enters focus. This does not change command security, healing or resurrection rules.


## Bot command conversation separation (2026-10-17)

Outgoing chat-packet observation excludes addon-language traffic and recognised real-player bot commands before conversation recording and stock/LLM reply queues. ExternalEventHelper recognition mode shares native command lookup/splitting but never fires an ExternalEvent; normal execution and security remain unchanged. Ordinary item/quest links remain conversation-eligible.


## Defensive master focus and fleeing enemies (2026-10-17)

Human-master PvE focus now prefers an existing fleeing combat target; then an attackable selected enemy already engaged by the master/group; then the current/native group attackers for defense. Selection alone never authorises a pull. Native attackability, tapping, immunity and CC checks plus sight range/LOS remain enforced. Unsafe fleeing pursuit pauses rather than switching; death/end of fleeing releases the lock. The native attackers value is used directly to avoid recursion through possible attack targets. Existing healing priorities, security and PvP/free-bot behaviour are unchanged.


## No pursuit of fleeing enemies (2026-10-17)

Human-owned companions never pursue hostile fleeing units. Melee bots exclude runners from attack/damage selection and may defend against other engaged attackers. Ranged bots retain a fleeing target but must use existing casting/shooting range and LOS from their position. Reach/ChaseTo/MoveNear/MoveTo unit paths share the restriction; active chase is cancelled by the existing StopMoving API. A transient pursuit GUID records the target behind hazard-routed MovePath, whose generator has no target; movement updates clear it and ChaseTo sets it, StopMoving clears it. Pet attacks on runners are cancelled too. The no-pursuit check remains active while the human master is dead; native PvP and free-bot rules remain unchanged.


## Safe party corpse recovery (2026-10-17)

Automatic corpse reclaim for active human-owned PvE companions requires a living, nearby, same-map/group leader out of combat with LOS, and no live hostile inside its native aggro radius at the bot revival position. A fresh unvalidated no-LOS discovery avoids ghost visibility/attackability and cached-target omissions; native WorldPosition aggro distances decide safety. Ghosts regroup toward their owner by default rather than independently running to the corpse. The native reclaim handler still enforces ghost state, delay and corpse radius; rejected requests no longer report success or clear corpse-run state. Explicit corpse run changes movement only, not reclaim safety. Explicit spirit-healer revive and accepted player resurrection requests retain native semantics. Automatic self-resurrection uses the same safety gate; explicit owner self-resurrection is preserved. Free bots, battleground recovery and real-player behaviour are unchanged.


## Healer damage mana reserve (2026-10-17)

Active human-owned bots with a healing strategy may start damaging spells only above
50 percent mana. The shared IsPlayerMasterSpellAllowed hook rejects native direct
damage effects and damaging aura applications at or below 50 percent, including
mixed healing/damage spells. It is evaluated by the existing action isPossible and
actual cast paths, independently of the living leader focus rule. Pure heals,
resurrection, dispels and other utility keep their native rules; existing DoTs are
not removed. Native ranged auto-repeat (wand Shoot) is exempt and ordinary melee
attack permission is unchanged. Priest offdps queues Shoot regularly below its
spell priorities, rather than waiting until the no-mana trigger, so a learned wand
attack remains available while spell damage is refused. Native heal priorities,
target/LOS/range, fleeing pursuit and free-bot behaviour are retained.

Regression coverage in tests/playerbot-master-focus extracts the actual policy:
0/49.9/50 percent reject damage and DoTs, 50.1 permits them; mixed positive spells,
wand, heal, utility, melee, dead leader, non-healer and free-bot cases are covered.
Gameplay acceptance must still check Mercy/Cria during a real fight.

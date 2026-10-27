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

### Script-generated NPC gossip

Explicit playerbot `talk` must reach native HandleGossipHelloOpcode before
deciding whether choices exist: scripts/modules/Lua can create them without
gossip_menu_option rows. Keep database-menu admission for idle RPG gossip to
avoid expanding autonomous quest interactions. User-selected replies reuse the
native menu and selection handler; empty menus and another NPC's current session
menu must not be indexed. [Native regression and scope](playerbots/SCRIPTED_GOSSIP.md).

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
Bots without Mining/Herbalism silently skip those nodes. An explicit use command
with a trained but insufficient profession reports the formatted node and native
current/required skill. Both cases leave selected loot untouched and do not
attempt open loot. Automatic gathering remains silent. Other
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


## Owned engineering explosives (2026-10-17)

ThrowGrenadeAction now selects a real carried explosive for active human-owned
PvE companions. Player::CanUseItem(Item*) enforces the item's actual level, skill,
spell, class/race, binding and reputation requirements. Inventory trade/empty
stacks, skip-spell entries, missing spell data and item cooldowns are excluded.
Selection ascends ItemLevel, RequiredLevel, RequiredSkillRank, ItemId, exhausting
lower usable tiers first. No engineering rank or player level is invented.

Only destination-centred damaging blast explosives with known native range and
radius are supported; special/caster explosives retain their dedicated actions.
The target must be a native group attacker allowed by master/fleeing/spell policy
with LOS and range. A fresh unvalidated local scan rejects live CC or unengaged
bystanders in the blast. It does not chase to obtain range or open combat.
The existing AoE strategy registers a five-second owned-explosive combat trigger,
including single targets. Disabling AoE still disables this automatic scheduling.
Healing bots, mounted bots and active casts are excluded. Free bots and PvP retain
legacy grenade rules/selection; no potion or buff-item behavior changes.

UseInventoryItemOnly forces possession and the existing native SetCastItem path,
even with item cheats: native Spell owns cooldown, cast time and consumption.
No parallel scheduler, persistence or inventory mutation is introduced. Existing
action diagnostics remain available. Extracted policy tests are separate from
full core compilation and in-game item consumption acceptance.


## Wand-to-spell handoff (2026-10-17)

Turtle Unit::_UpdateAutoRepeatSpell renews the spell GCD for every wand shot.
Higher action priority alone cannot preempt this: CanCastSpell uses the native
strict CheckCast, which rejects an active GCD before the spell reaches casting.
CastSpellAction therefore adds the existing interrupt-current-spell action,
qualified as wand-only, to its prerequisites for a known, policy-allowed,
non-auto-repeat spell while a wand is active. Native reach prerequisites remain.
The qualified interrupt cancels only the wand slot, copies the spell ID before
native cancellation, and waits at least the remaining ranged swing/configured
GCD plus reaction delay. It does not clear cooldowns or claim the heal/DPS spell
was cast. The original action remains queued and rechecks all native cast rules.
Unknown spells still use alternatives; policy-blocked damage does not stop a
low-mana healer's wand; hunter auto shot and unqualified interruption are unchanged.
Focused source-extracted tests: tests/playerbot-wand-handoff. This addresses a
source-level starvation mechanism; live healing, target/fleeing acceptance and
Alon's DPS/backup-healer configuration remain separate and unverified.


## Party battle sessions (2026-10-18)

BattleLog hooks native attack intent, accepted casts, resource mutation, damage,
healing, threat and movement without selecting AI targets or changing actions.
A human attack starts one party window; native combat flags across all roster
members and observed pets end it. Recorder state contains scalar snapshots only
and is serialized across map owners. Cross-map state is not dereferenced; gaps,
roster changes, logout, shutdown and caps mark captures incomplete.
Ordinary fleeing and assistance movement are distinct native states and both
are observed. Full contracts, controls and reporting: PARTY_BATTLE_LOG.md.

### Casts already active at battle opening (2026-10-18)

The first live captures exposed spells prepared before the human attack whose
diagnostic cast ID was unset. Their native mana loss and damage were visible,
but cast-specific launch/cost events were dropped. The adapter now attaches a
diagnostic ID at the first relevant later phase and records `cast_attached`;
it does not replay preparation, invent a start time, or charge power again.
Item destruction retains the supplied item entry even after native item-pointer
clearing. No session is opened by attachment, and no AI decision is changed.
Reports distinguish structural completion from resource attribution, warn about
initial/attached casts and unattributed pool loss, and withhold efficiency ratios
for affected characters. Pool loss is not assumed to be spell cost. Existing raw
captures cannot recover missing cast phases. Focused adapter/report tests run on
Windows; the full native build and gameplay validation await a server-off window.

## Coordinated human-party healing (2026-10-18)

Human-owned PvE companions use current same-map group casts to estimate pending
single-target direct healing. Preparation, live caster/target, remaining cast,
LOS and affordable power are required. Read prepared base points and native
healing-done/taken bonuses without a Spell argument: DropModCharge is a no-op
without that argument. Prediction does not roll RNG or run spell scripts/procs,
consume mana/items or retain pointers. Crits, extra dice/level scaling, channels,
HoTs, shields and Turtle dummy-triggered heals are not invented as guaranteed
coverage. A native melee-swing pressure allowance and critical-health override
allow multiple rescuers when required; ranged pressure and scripted healing are
not fully predicted by this first change.

The existing party patient selector accounts for pending coverage and pressure
instead of distributing by healer roster index. Injured critical patients win;
at equal urgency a tank wins the tie. Direct-heal action usefulness and actual
queued execution recheck coverage. HoT/shield policy, explicit cast commands,
free bots, PvP and the 50-percent offensive-spell cutoff retain their boundaries.

The existing heal-interrupt reaction is added after saved strategies load for
owned parties. Ordinary heal cast bars are PREPARING, not the native channel
CASTING state. A heal-qualified native interrupt rechecks the patient's health
at execution and copies spell identity before cancellation can destroy it. It
never clears GCD or charges/refunds power itself.

A DPS bot with offheal supplies emergency direct healing only: the patient
must be at the configured critical-health threshold or within one estimated
lethal melee swing. Moderate injuries do not recruit the backup even when the
priests are out of mana. A heal already underway on the patient (including
partial coverage), or an available living same-map primary healer with a known,
ready, affordable heal in range/LOS, keeps the backup on damage/protection.
Critical health does not override that role preference. Priests busy on other
patients, dead, silenced, out of range or unable to afford a heal permit emergency
backup. Native wand auto-repeat does not make a primary unavailable; its existing
qualified handoff stops shooting. Vanilla Paladin Flash of Light is offered at
the critical tier; Lay on Hands also passes the backup gate. Ordinary primary
healers retain their critical rescue override. This refinement is source-only
until the next stopped-server build; the deployed 8d6af717 predates it.

The owned melee backup Paladin may override master focus to peel an already
engaged, attackable, LOS-valid enemy currently within melee reach of a grouped
primary healer. CC/runner policy remains enforced. Once holding that enemy it
keeps it until the tank recovers its attention; distant ranged attackers do not
qualify for this pursuit exception. No new roaming or pull targets are acquired.
Alon's role is a separate reversible saved-strategy configuration: Retribution
plus offheal rather than Holy, retaining his talents, gear and professions.

Focused tests compile extracted production helpers, selector, interruption and
action methods. Cover sufficient/partial pending healing, critical and incoming
pressure, same-map ownership, unavailable primary healers, native dummy-effect
Paladin heals, stale cancellation, healer peeling and return to master focus.
Windows local tests passed; full Linux build and gameplay acceptance remain
separate. Retest the 410-mana waste example and Priest/Paladin healing roles.

The cancellation reaction is installed for owned PvE companions even before a
group invitation, because SetMaster/ResetStrategies precedes later invitations.
Coordination itself still requires a group. Explicit `preheal` keeps its native
patient/cast policy and does not acquire automatic full-health cancellation;
manual heal-interrupt configuration remains authoritative. Extracted lifecycle
checks cover group-less login, later grouping, preheal, free bots and PvP.

## Combat and recovery refinement from Alin captures (2026-10-18)

The 348673fe playtest exposed a graveyard/corpse direction loop, unavailable
emergency Flash of Light after grouping, and near-full prepared heals whose
GetDamage remains zero. This revision addresses those responsible native paths.

An explicit playerbot `revive` sets transient `spirit healer requested` in the
existing AI context and cancels `corpse run` movement. The existing dead-engine
`should spirit healer` trigger continues the requested action at its native
priority until recovery; find-corpse and body reclaim yield while it is active.
A new `corpse run` cancels the graveyard request and cached continuation value.
The alive transition clears both instructions, including after an accepted player
resurrection. No new scheduler, saved DB field or resurrection bypass is added.
The existing spirit-healer travel, resurrection sickness/durability penalties and
reclaim safety remain; released-ghost automatic spell selection is not changed.

Owned offheal Paladins install an emergency trigger even before invitation and
evaluate group/role/availability at runtime. Fast healing is eligible at the
configured lowHealth threshold or one estimated lethal melee swing, rather than
waiting until criticalHealth. Pending primary healing still owns the patient;
ready primary healers suppress backup. The availability scan uses the rank the
healer's native spell-id resolver actually selects, rather than an affordable
lower rank its actions will not cast. Routine healing remains the priests' job.
Native spell checks still own mana, range, LOS, cooldowns and actual success.

Prepared direct-heal estimation is shared by pending coverage and cancellation.
Above 90 percent health, a cast with less than half its conservative direct-heal
estimate needed (including melee pressure) may be cancelled by the existing
reaction, rechecking at execution. No RNG, script, modifier-charge mutation or
Spell pointer is passed to bonus helpers. No direct estimate is invented for
Turtle dummy heals, HoTs, shields or channels. This does not select lower ranks
or promise perfect future-health prediction.

Paladins may use pure stun control on a native engaged fleeing attacker already
within spell range and LOS. The existing Hammer of Justice snare action is used;
the spell-policy exception rejects damage and preserves CC/tapping/attackability.
No attack-focus, chase or reach restriction is relaxed. Native cooldown, mana and
immunity checks decide whether the spell actually casts. Free bots/PvP are kept
on their existing paths. Automatic Blink is suppressed for human-owned PvE bots
until landing-position safety exists; explicit cast commands remain available.

Battle observation skips a same-map near-teleport acknowledgement and resumes
the same encounter, while actual departure/far teleport still closes it. This
does not retain or dereference units belonging to another map owner.

Regression coverage compiles extracted production recovery commands/guards,
healing estimates/selector/interrupts/emergency trigger, master focus/control,
Blink and battle observation. These tests stub native geometry/handlers and do
not establish gameplay success or all-encounter balance. Alin's full-party wipes,
runner-control opportunities and one-command graveyard recovery need playtesting.


## Heal continuity, timely rescue and bounded pursuit (2026-10-18)

The b74fc343 Alin capture exposed a useful friendly Heal interrupted by hostile
retargeting, a delayed self-heal suppressing backup until death, and an AI
patient-range cutoff narrower than learned heals. SelectNewTargetAction now
yields while an owned coordinated party heal is preparing on a live injured
same-map patient (including self). Existing qualified overheal cancellation and
native spell checks still own cancellation/completion.

Pending direct healing is counted only if the patient can survive estimated
native melee swings before its current remaining timer expires. Primary healer
availability applies the same check to native modified cast time. Mean native
melee damage and attack timers supply the estimate; ranged damage, DoTs, crits,
armor, absorption and future movement are not forecast. An unsafe estimate
allows emergency cover; it never makes a cast succeed or clears a GCD. Timely
partial healing retains the backup role preference. The learned healing range
extends party selection beyond the preferred 30 yards when a known native heal
can reach farther, capped at sight distance. Free bots, PvP and preheal retain
selection/coordination boundaries.

Owned-PvE focus requires actual enemy combat contact even if the human already
has an attack victim set. Hostile pursuit is limited to the existing follow
range around the same-map group leader; movement provenance cancels stale
chases. Stationary defense is separate from pursuit permission, preserving
self/party defense and fighting after the leader dies. A non-breakable stun
alone permits finishing an engaged target. Breakable CC, fear/root, LOS,
attackability and tapping remain checked. A stunned runner may be struck in
melee reach without pursuit; ranged bots retain their finish-from-position lock.

See PARTY_COMBAT_SAFETY_2026-10-18.md for battle evidence and validation limits.


## Gathering follower safety consistency (2026-10-18)

Automatic gathering previously rejected grouped followers for any hostile within
an extra hard-coded 30 yards, before the shared loot admission path applied
LootHostileDistance (20 on the test VM). Grouped followers now use that existing
shared safety check, including its configured opt-out and later selection/movement
rechecks. Solo/group-leader gathering retains its native 30-yard strong-hostile
policy. Gathering discovery range, LOS, native profession/tool/loot eligibility,
bag capacity and spell opening are unchanged. Explicit `u go` retains its native
manual interaction path; it does not establish automatic admission eligibility.

Opt-in debug-loot gathering rejects now identify LOS, loot policy, gathering
range and solo-hostile failures. The shared parent reports configured hostile
blocks. No new UI chatter, configuration, logging stream or scheduler is added.
This corrects a proven policy inconsistency; the old Alon action log only said
FAILED and does not prove which filter rejected his particular Copper Vein.

## Companion equipment and buff-item decisions (2026-10-20)

CanPlayerMasterChase accepts engaged enemies inside the human master's native
melee reach as well as the existing follow offset. A master can fight at normal
melee reach while the companion catches up from behind; selection alone still
does not authorize a pull. Fleeing targets never authorize pursuit.

ItemUsageValue selects ring/trinket/dual-wield destinations by comparing legal
native CanEquipItem candidates, rather than assuming NULL_SLOT selects the worst
item. Roll and automatic-equipment classification share this destination and
include random properties/enchantments even for equal base item IDs. Miscellaneous
jewelry/held slots are separate from armor-material preferences. Native class,
level, skill, uniqueness, binding and inventory constraints stay authoritative.

Upgrade scans retain current hand equipment, refresh candidate classifications
after mutations, and verify the actual native equipment result before reporting
success. Two-handed upgrades compare against the combined equipped hand scores;
an offhand alone cannot evict a two-hander. The scan is conservative: it does not
search every possible pair of currently inferior bag weapons for a combined upgrade.
Player Need still takes priority over bot Need, and feedback describes the final
vote. Irrelevant damage-school bonuses no longer erase useful base caster stats.
Existing stat-weight data is retained; this is not a new universal gear ranking.

AiPlayerbot.AutoBuffConsumables defaults to 1 and installs the existing non-combat
consumables strategy for human-owned PvE bots after legacy saved strategies load.
Set it to 0 to disable automatic addition; an explicitly saved consumables strategy
remains a separate user choice. Free bots, real players and PvP defaults are unchanged.

Generic classic consumables are classified through their spell effects. Combat
health regeneration is recognized; physical armor buffs are distinct from reserved
elemental protection items; stat scrolls are not blocked by a food buff. Selection
uses real carried items, native cooldowns, native spell-group/rank exclusion and
self targeting. Lower usable tiers are selected first, existing conflicting effects
are retained until expiry, and irrelevant Strength/Agility buffs are skipped by
casters/healers. A rejected candidate does not starve unrelated usable buffs.
Emergency healing/mana potion scheduling remains separate from this preparation.

tests/playerbot-gear-consumables extracts the production classification, destination,
scan, equip-dispatch, buff-selection and strategy-reset methods. The master-focus
fixture covers native melee contact outside the follow offset and retained pursuit
limits. These fixtures do not establish gameplay, native navmesh or live aura-data
acceptance; test the actual party after deployment.

## Classic group-roll equipment notification (2026-10-20)

Flore won Shimmering Gloves (6565) at 16:01:08 UTC on e7aa6ff5, but no
equip-upgrades action followed. Native Group::CountTheRoll stores Need/Greed
awards and calls OnReceivedItem without SendNewItem; OnReceivedItem only handles
save/log policy. The bot previously listened for SMSG_ITEM_PUSH_RESULT only.

The Classic bot adapter now queues SMSG_LOOT_ROLL_WON through its existing
packet/trigger engine and invokes the existing gear scan only for the winning
bot. The native award, roll votes and item-store contracts are unchanged. The
notification is not permission to equip the rolled template: the scan evaluates
real carried instances and still uses native equipment restrictions. Duplicate
events are harmless, other winners and truncated packets are rejected. Combat
item restrictions remain authoritative. This packet layout is Classic-only;
TBC/WotLK registration is unchanged. No new timers, logs, SQL writes or synchronous
inventory changes inside the packet-send hook are introduced.

The regression constructs the native 34-byte Classic wire layout, delivers only
the win notification (no item-push), and checks actual scan execution, foreign
winners, short packets and repeats. Full server build and gameplay remain pending.

## Two-handed upgrade eligibility with an occupied offhand (2026-10-20)

Cria's saved Blackrock Mace plus Runic Stave scores 0 + 68 with the Holy scale;
Lesser Staff of the Spire scores 198. She has Staves skill. Native CanEquipItem
rejects a two-hander with an equipped offhand when not_loading=false, before
the comparison. That mode intentionally avoids implicit offhand displacement
during loading; it does not answer whether a live upgrade can be equipped.

GetEquipDestination requests live native placement validation for a two-hander
with an occupied offhand, for both real carried instances and unseen roll
items. CanEquipUnseenItem forwards that optional flag; its existing default is
unchanged for other callers. Native CanUnequipItem and CanStoreItem must succeed,
so a locked offhand or lack of bag space still rejects the change. The comparison
still includes the combined equipped hand scores. No forced unequip, item-specific
exception, skill training, scoring-data change or inventory rewrite is added.

The focused regression fails against the old caller and passes after correction;
carried and unseen replacements and rejected offhand storage are covered. Full
native build, Linux checks and gameplay validation remain pending.

## Play-session loot follow-ups (2026-10-21)

Native LootHandler::DoLootRelease owns mineral-vein use counts, random depletion,
GO_READY versus GO_JUST_DEACTIVATED, and loot clearing. ShouldLootObject now
recognizes a spawned, idle READY mining vein even when its previous generated
loot is empty. After actual item transfer and native release, StoreLootAction
re-admits that same vein through AddGatheringLootAction and the existing bounded
LootTargetRetryDelay. Normal gathering strategy, master range, skill/tool, bags,
LOS and hostile admission remain authoritative; combat/death/in-use/depletion
prevent automatic continuation. Herb and ordinary-container lifecycles remain
native. No fixed use count, forced state transition or new timer was introduced.

AiPlayerbot.AutoLootSkipItems is a configurable default item-policy exclusion:
20516 (Bobbing Apple), plus Turtle Classic 51751 (Bone Chew Toy). Classic's
custom ID is not applied to TBC/WotLK defaults. Active quest needs and the existing
always-loot and forced Need/Greed overrides take precedence; existing explicit skip-list precedence
is unchanged. Source admission declines only known templates consisting wholly
of those excluded items, preserving unknown and mixed loot. Existing inventory
is not deleted. The initial confirmed sources are Apple Bob (300203) and
Bone Chew Toy (1000380), not blanket seasonal/quality/name categories.

Follower loot safety retains the configured native 3D radius. Within that radius,
idle hostiles without native geometric LOS to the destination no longer block
it solely through walls/floors. Engaged enemies remain conservative. The shared
hostile-unit query and creature aggro mechanics are unchanged. This destination
check does not establish path-wide safety or repair missing collision geometry.
Source-extracted loot-followups, gathering-safety and loot-safety regressions
cover these contracts; full image compilation and live acceptance are separate.

## Party-bot quest snapshots and pfQuest extension (2026-10-21)

The exact LANG_ADDON `PFQBOT` prefix is a read-only request protocol, not ordinary
bot chat. PlayerbotMgr's existing authenticated chat ingress reaches
PlayerbotAI::HandleCommand; only the named bot's real-player master in the same
group can enqueue a valid version-1 request. A five-second atomic admission gate
and one-pending flag bound ingress. A private native ChatCommandHolder carries
the requester's GUID rather than retaining its Player pointer. HandleCommands
resolves that identity again on the bot's serialized AI owner and rechecks
master/group authorization before reading quest slots/status/counters. A second
send-rate gate bounds replies. Logout, master change and group departure cancel
access. Capture does not invoke a gameplay action, interrupt casts, traverse a
foreign player's quest map, query SQL or mutate quest state.

QuestSync's adapter reads actual slots first, retaining active repeated quests
even when earlier reward history exists. Requested IDs absent from the log use
the native saved-in-memory reward flag for historical R versus absent N; absence
alone is never a turn-in. Read-only map find operations avoid inserting status
rows. Item and creature/GO/cast counters, exploration and required money preserve
their native meanings; ready/failed remain native quest states.

Player::SendAddonMessage provides the existing LANG_ADDON local-recipient GUILD
packet shape; this does not broadcast to a guild or use human whispers. Replies
contain version/token/bot/hash/part metadata with 128-byte payload chunks and a
192-chunk maximum. Unchanged data sends one freshness packet. The client accepts
only its own native sender identity, an outstanding token and a current roster
member, validates the complete bounded data snapshot, then atomically replaces
the last valid cache. Partial/malformed/timed-out responses retain stale data.
The hash is an equality tag, not a security primitive.

The separate [pfQuest-partybots repository](https://github.com/pjw345/pfQuest-partybots)
contains the AddOn, client tests and release downloads. It wraps the installed pfQuest 8.0.1 ButtonEvent,
ButtonEnter and DoLayout surface. Personal quest data, pfQuest database/maps,
normal row interactions, Quest Log bindings and helper commands remain native.
Bot-only rows use a companion footer/panel, never synthetic personal quest slots.
Current roster names are derived from client unit APIs. The comparison includes
stable quest IDs, while tracker rows show compact per-bot progress and hovers
show objective counts. Freshness/offline/unknown states stay explicit.

tests/playerbot-quest-sync compiles the production adapter/protocol with host API
fakes and can emit actual native packets. The companion repository owns the Lua
decoder tests and consumes those packets via --wire. Lua 5.0.3 and 5.1 tests can
extract the installed pfQuest's unmodified row/layout functions. See
`docs/playerbots/PFQUEST_PARTY_PROGRESS.md` for installation and cross-repository validation.
These checks are separate from full native image compilation, socket/client
AddOn delivery, actual graphics and live quest-progress acceptance.


## Collision asset identity and Tower of Azora (2026-10-21)

WMO extraction must retain complete source-path identity. Vanilla and custom Turtle assets can share a basename; flattening them can bind a map spawn to the wrong geometry. Root outputs and ADT, WDT/global and game-object references now share `WmoOutputName`. Existing model-name substring controls and the native Zeppelin adjustment are preserved; M2 naming and native collision/navigation formats are unchanged. See `docs/navigation/AZORA_WMO_COLLISION_REPAIR.md` for provenance, native reader/writer repair, the two Azora tile references and migration limits. A full fresh extraction must regenerate matching metadata/navigation together; never mix new names with legacy references. Offline connectivity success is not live movement acceptance.

## Vanilla quest request transport correction (2026-10-22)

Live testing found `Core.lua:126: Invalid escape code in chat message`: the
1.12 client validates pipes in SendAddonMessage before transmitting. Requests
now use `Q:1:token:bot:hash:questIds`. ParseRequest accepts that complete envelope
and retains legacy `Q|1|...` input; mixed delimiters and doubled-pipe input are
rejected. Snapshot/reply formats, queued GUID ownership, native capture, rate
limits and size/row bounds are unchanged. This corrects the transport rather
than bypassing the client's validation or parsing human chat.

Companion 1.0.1 requires this parser; deployed image 2151c15b does not accept
colon requests. Focused tests can run on the Windows workstation while playing;
full server compilation/activation must wait until the game services are stopped.
Live transport and UI acceptance remain pending.

## Bounded WMO tile coverage and Stormwind (2026-10-22)

Per-tile collision metadata must include every existing bounded, non-global WMO placement whose bounds overlap that tile, even when an ADT overlay omits a repeated placement record. `TileAssembler` now completes these references without changing native models, spatial-tree indices, transforms, M2/global handling or runtime collision/movement policy. See `docs/navigation/STORMWIND_WMO_TILE_COVERAGE.md` for the demonstrated missing Stormwind street surface, native-format scoped repair, matching navigation regeneration and live acceptance limits. Keep the deployed map parameters and earlier Azora repair; do not substitute teleporting or relaxed collision checks for missing geometry.

## Prepared session fixes (23 October 2026; not compiled)

Branch `codex/session-followup-fixes` contains source drafts for #58, #66 and #67.
No configuration, CMake generation, compiler, test executable, image build or
runtime activation has been run for these drafts.

- Classic gathering admission reuses `GameObjectInfo::IsRepeatableGatheringChest`,
  the same predicate now called from the existing native opening-spell LOS
  exemption. Non-Classic bot variants retain their previous LOS check. Ordinary
  chest LOS, approach safety, skill/tools, range, bags and native depletion are
  unchanged. Live evidence showed manual mining succeeds where admission rejected
  the same node after loot release. Full build and repeat-mining acceptance remain.
- Buyback captures item identity/count before native storage, then confirms an
  emptied buyback slot and increased carried item count before telling the requester
  the actual recovered quantity. Failed storage/money/vendor cases cannot emit a
  success confirmation. Native handler/prices remain authoritative (#66).
- Enchanting tool checks accept higher-tier runed rods from the five verified
  Classic/Turtle item definitions. Exact other-tool checks and consumed-reagent
  checks remain native; bank-only rods are not added to the carried predicate.
  Client initiation/UI and trade enchanting still need verification (#67).

`tests/session-followups` contains future regression source, not executed results.
See `docs/playerbots/NEXT_BUILD_20261023.md` for pending work and validation.

- Item-started bot quests: `UseAction::UseQuestGiverItem` passes the actual owned item to `Player::AddQuest`. Native acceptance retains required/source starters and consumes other starters while preserving item quest-accept hooks. Do not unconditionally destroy the starter afterward: An Unsent Letter (2874, quest 373) is also its delivery objective.

- Transport follow: compare actual native passenger attachments before replacing ordinary follow. Different stored movement GUIDs alone do not constitute a transfer. Movement packets without ONTRANSPORT clear their omitted transport payload; native boarding/removal hooks remain in HandleMoverRelocation.

## Activated quest-chest availability (2026-10-25)

Loot::HasQuestLootForPlayer is a read-only counterpart to FillQuestLoot and the
quest branch of LootItemInSlot. Existing per-player slots retain their taken
state and current AllowedForPlayer conditions; a missing view checks remaining
shared quest items and the native slot-capacity limit without populating slots,
adding allowed looters or changing unlootedCount. Bot activated-chest reopening
and serialized turn selection use this query. READY chests still use template
eligibility and native generation. Shared non-FFA quest loot is not duplicated.

Native release remains responsible for global exhaustion and deactivation.
A crate with another player's counted item remains ACTIVATED; taking the final
item makes isLooted true, then release deactivates and normal update schedules
the configured respawn/restock. Do not erase other players' loot or force
respawn merely because one player's window is empty. No new timer is added.

## Successful mining repeat pacing (2026-10-25)

QueueNextMiningAttempt runs only after a verified item transfer and native
loot release. Successful continuation uses the existing lootDelay, rounded up
from milliseconds to whole seconds for Defer, with a one-second minimum.
lootTargetRetryDelay remains for actual opening/storage failures. Preserve
native casts, depletion, master/range/safety/tool/bag admission and cancellation.
Do not shorten the global reaction/GCD or force gathering through follow/recovery
conditions merely to make node collection quicker.

## Level-aware loot safety (2026-10-25)

Follower loot admission, queued selection and approach rechecks share
LootObject::IsApproachSafe. Idle hostile creatures use native
Creature::GetAttackDistance(bot), retaining level scaling, native detection and
detected-range auras, server aggro rate, custom detection ranges and native caps.
The target is the bot that approaches, not its master. Non-initiating creatures
and zero native aggro do not block merely by proximity. Idle LOS/floor separation
remains; engaged enemies retain the configured combat exclusion radius as a minimum
while also respecting a larger native aggro/interaction radius.

Candidate discovery covers the native visible area, including area overrides (or a larger configured
combat radius), so normal higher-level aggro is not truncated at the old 15-yard
scan. Idle threat distance adds the existing INTERACTION_DISTANCE to protect
possible interaction positions around a node and uses native centre distance.
This is conservative destination-area safety, not proof that every path segment
or scripted encounter is safe; native pathing, cast and combat checks remain.
No new global hostile query behavior, level table or timer is introduced.
Debug loot logs the bot/hostile levels and effective aggro/safety radii.

### Group resource minimap feed

RESBOT version 1 is a silent opt-in, read-only gathering feed. Incoming party/raid
requests pass the same controlled-master/group authorization as quest sync and a
separate five-second admission gate. GUID-only requests are processed through the
existing bot command queue; current requester/group/world state is revalidated on
the serialized bot owner. No object pointer or world scan is retained on the chat
handler. Native lock requirements, current Mining/Herbalism skills, spawn/loot
state, both players' native visibility, bounded discovery range and native
Map2ZoneCoordinates control eligibility. Nearest 32 eligible nodes are returned
as at most 16 packets, each at most 240 bytes. Empty snapshots clear old state.
No loot target, movement, cast, strategy, player tracking field or skill is changed.
An active client request grants a twelve-second lease suppressing legacy resource
reveal speech/pings; other uses of Ping are unchanged. Without a compatible client,
the legacy reveal behavior remains available. The client uses pfQuest's installed
minimap dimensions/zoom support for separate, non-clickable ore/herb textures,
deduplicates GUIDs as strings, expires data and clears on roster/zoning events.
Unsupported instance coordinates fail closed. These markers express profession
eligibility, not a promise that a path is safe or a node immediately gatherable.

### Custom game-object catalogue and novelty source exclusion

The CMaNGOS sGOStorage compatibility proxy now reports an exclusive upper bound
from Turtle's native ObjectMgr catalogue instead of the old 200000 stub. Apple
Bob 300203 and Bone Chew Toy 1000380 were omitted by DropMapValue's source scan,
leaving the earlier excluded-only source check with an unknown/empty item list.
Actual item storage still rejected apples, explaining travel without pickup.
Native maximum + one includes the highest template. Existing sparse lookups and
all source-policy overrides stay intact; no object ID/name blacklist was added.
Other catalogue consumers (travel purposes, random-item source data, transport
generation) now see custom templates too. These are startup/lazy catalogue scans,
not per-AI-tick loops; sparse high IDs increase scan work. The current read-only DB
snapshot has 21196 templates, maximum 7000035, of which 12091 are above the old
bound. Source-extracted tests cover the catalogue pipeline. Full image build,
startup cost and gameplay acceptance remain pending, with no live mutation.

### Player and bot resource union

The helper has one On/Off control; On always requests both supported resource
types and merges the human player's own snapshot with controlled bots' snapshots.
RESSELF requests use native guild addon transport, intercepted/consumed by the
module before guild membership/broadcast handling (including solo/unguilded users).
The session chat hook only deposits a bounded request in the current human's
manager mailbox. Native player map updates consume it and scan only that player's
skills/world. No grid traversal occurs on the session handler. The mutex mailbox
admits one pending request at most per five seconds; logout destroys its owner.
The self feed does not require a group. Bot feed authorization remains unchanged.
The client keys self snapshots by its own name, separately validates grouped bot
names, merges string GUIDs, and clears stale or departed contributors normally.
Only Mining/Herbalism game-object resources are supported; skins/corpses are not
introduced as minimap resources. Native tracking auras and professions are untouched.

### Prompt first gathering discovery

Live Alon action logs on 18d7060b showed follow(priority20) repeatedly winning
while add-gathering-loot(priority2) waited; follow decisions were about2.9 seconds
apart. Once the scan ran at14:18:27.870, native loot selection/opening followed
within100ms. Discovery now has priority ACTION_HIGH+1, below existing loot/move/
open and higher emergency actions. Native timer cadence remains once per second;
range, profession/tool, bag, LOS, safety and leash admission are unchanged. An
ineligible scan still fails and lets follow proceed normally. isUseful preserves
casts/sitting/death and delegates ready food/drink checks to the existing native
triggers/actions/inventory values; no consumable thresholds are duplicated. The
successful-repeat pacing accepted by the user is unchanged. Existing action waits,
native cast time and other activity can still contribute to response time.

## Collision placement identity (2026-10-25)

The native assembler distinguishes MDDF/M2 from MODF/WMO when deduplicating
placement IDs and assigning tile/tree references. Custom ADTs can reuse an ID
across these types; the Dun Morogh tunnel UID 5525455 previously aliased a
distant Westfallshed M2. Serialized IDs/formats and runtime ownership remain
unchanged. Same-type duplicate IDs keep first-record semantics. See
`docs/navigation/DUN_MOROGH_TUNNEL_REPAIR.md` for reproduction, migration
constraints, focused staging and offline/live verification boundaries.

## Far-formation rear geometry (2026-10-25)

FarFormation::GetAngle requests a stable pi-radian rear offset. Getters must
not interpolate using the active follow generator's last requested angle: this
created alternating requests and repeatedly replaced the native controller.
Native target selection, saved distance/radius conversion, prediction, pathfinding
and speed/talent handling remain unchanged. See
`docs/playerbots/STABLE_REAR_FOLLOW.md` for the production getter reproduction,
source-extracted follow/dispatch regression, and pending full build/live checks.

### Native player movement state for casting and interaction

On this Turtle bridge, player splines can move with UNIT_STAT_MOVING unset.
Use native WorldObject::IsMoving at the ZERO facade boundary; IsStopped refers
to legacy generator state and is not an equivalent translation predicate.
Otherwise the existing cast preparation can skip StopMoving and native spell
movement interruption can cancel gathering. Keep native cast/stop/recovery and
other expansion predicates authoritative. See playerbots/NATIVE_MOVEMENT_STATE.md.


### Body-loot to skinning transition (26 October 2026)

The existing per-bot ignore cache distinguishes ordinary body inspection/rights
suppression from real failures and completed skinning. A marked body suppression
may end early only when native skinning ownership and full-body-loot readiness,
unused skin loot and existing skill/tool eligibility all permit the next phase.
Unchanged bodies, other objects and real failed-skinning retry periods retain
their timers. No native ownership/outsider timer or loot generation is changed.
The first queue-only fix was built/deployed as 4e848db2, but the first live
boar still produced no body loot or skinning before despawn. Native loot-rights
rejections alone do not identify an owner: IsAllowedToLoot also rejects an
already-empty body. Turtle Unit::Kill sets LOOTABLE from template presence,
including empty rolls. ZERO corpse classification must therefore inspect native
loot.isLooted() before giving that flag priority over SKINNABLE. Keep non-empty
body loot first, native skinning ownership/readiness/tool/skill gates intact,
and other variants unchanged. Source-extracted real corpse classification and
queue fixtures cover these complementary paths. The classifier extension still
requires a full image build and live acceptance; the exact live corpse's native
loot contents/dynamic flags were not captured before it despawned.

Ordinary body rejection also uses an availability marker separate from successful
body inspection. ZERO may release only that rejection marker when the native body
still contains loot and current ownership/item-policy checks now permit it. This
recovery does not require the profession gather strategy. Successful inspection,
actual failure/retry, unchanged native denial and other variants keep their
timers. Live evidence shows Mercy's last rejection at 18:41:30, eligible discovery
by 18:42:30 and body opening at 18:43:30, matching the remaining ignore period.
The queue fixture validates native-predicate-controlled recovery and inspection,
failure, replacement and clear controls. The extension is source-only until the
next build; the running 4e848db2 skin transition has successful live casts.

### Grey party-kill loot admission (26 October 2026)

Turtle group kills emit SMSG_PARTYKILLLOG (killer GUID then victim GUID) even
when they grant no XP. The existing bot outgoing-packet queue delays repeat
events until their trigger can consume them. ZERO now maps that native event
to normal AddAllLootAction admission for living loot-enabled companions with an
active human master in the same group. Existing ownership, distance, safety,
bags and profession opt-in still decide admission; selection/combat checks,
movement, native opening/casts and corpse lifecycle remain unchanged.
A successful new admission invalidates only the cached has-available-loot value.
The five-second scan and XP path remain fallbacks; no global timer is reduced.
Other bot variants retain their existing registrations. Source-only until build.

### Insufficient companion Skinning skill warning (27 October 2026)

ZERO gathering discovery privately reports the native required/current Skinning
points when low skill excludes an otherwise locally skinnable corpse. Native
classification keeps its empty GUID and refusal; this feedback does not enqueue,
ignore, defer, move to, cast on or mutate the corpse. A read-only temporary target
reuses destination safety solely before reporting. The active human master must
share the group; the body must be looted, skin loot unused, native tap/skin rights
valid, knife present, and normal master gathering range, height and LOS valid.
Existing private/silent/security and text-repeat handling remain authoritative.
An independent existing LootTargetList remembers successful warnings per corpse
for LootTargetInspectDelay, then prunes its timestamps on the next eligible check.
Other variants retain existing behavior. No extra discovery, polling, database
query or always-on diagnostic stream is added. The cast's two-second duration,
native skill/tool/ownership checks and real loot retry state remain unchanged.
Blank manual-cast failure text is a separate compatibility issue (#98).
Source-only until a requested server build and live acceptance.

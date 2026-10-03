# Upstream audit through 2026-10-03

## Result

The accepted Playerbot sync build is a sound checkpoint, but it is **not yet a complete integration of everything applicable from the two active upstreams**.

This audit found:

- fixes already present locally under different commits or adapted interfaces;
- a finite queue of additional Classic/Turtle-compatible correctness fixes;
- optional behaviour changes that should be selected and tested rather than imported automatically;
- architectural, expansion-specific, diagnostic, test-only, documentation, and packaging commits that do not belong in the runtime merely because they are upstream.

The accepted build remains the baseline. The findings below are follow-up work, not a reason to reject it.

## Fixed audit scope

| Stream | Audited range | Tip date | Commits reviewed |
|---|---|---:|---:|
| Turtle WoW core | after shared base `9ab54c8ca2aa86ac3d90821743f8ec6c9802028d` through `d94947b0d` | 2026-09-29 | 55 non-merge |
| CMaNGOS Playerbots | after `993f18091e67565986cf55c4d9b8e6eae11223f9` through `14ccaa25b` | 2026-10-01 | 80 non-merge |
| Local comparison point | accepted merge `c6f6417eb03eb51216689e706382729be8258eb4` | 2026-10-03 audit | — |

The Playerbot lower bound is the latest upstream commit before the September sync window. The older documentation reference `811d6f1e` is not present in the current upstream object graph, so it cannot serve as a reproducible Git boundary.

Shyalya and Kasperfriend are lineage sources, not the two active upstream streams in this audit. Their useful changes were assessed in the earlier fork audit.

## Method and interpretation

The audit used commit enumeration, patch-equivalence checks, changed-file inspection, source-level searches, and direct comparison of affected functions. A clean `git apply --check` was treated only as syntactic evidence; it was not treated as approval to merge.

Statuses in this report mean:

- **Present/equivalent** — the behaviour or fix exists locally, possibly under a different commit or a Turtle-specific implementation.
- **Candidate** — applicable in principle, but must be adapted to current ownership, threading, module, database, or gameplay contracts.
- **Optional** — useful only if the corresponding feature or policy is wanted.
- **Superseded/incompatible** — conflicts with the chosen integrated `mod-playerbots` architecture or has already been replaced by a local design.
- **Not runtime applicable** — expansion-only, tests, diagnostics, packaging, or documentation rather than production Classic/Turtle runtime behaviour.

## Integration queue

### Priority 1 — correctness and crash safety

These should be handled before elective Playerbot behaviour work:

| Source | Commit(s) | Finding | Required approach |
|---|---|---|---|
| Playerbots | `8ef9193d9` | Quest auto-reward dereferences an empty reward-choice set. The local code still has the unsafe `*bestIds.begin()` path. | Adapt directly and add a no-choice quest regression test. |
| Playerbots | `e4b588836` | `SMSG_RESURRECT_REQUEST` is not registered as delayed locally, so a busy bot can drop the packet. | Small adaptation; test while another action wins the tick. |
| Playerbots | `b39956194` | External packet events can be discarded when their action does not run in the delivery tick. | Adapt the retention/release lifecycle carefully across normal and reaction engines. |
| Playerbots | `6ad783c95` | Local corpse handling has some stronger guards, but the zero-position guard is incomplete in `FindCorpseAction` and `ShouldSpiritHealerValue`. | Port only the missing checks; retain local terrain-safe graveyard lookup. |
| Playerbots | `89a4e5aeb` | Far teleport acknowledgement lacks the removed-battleground destination guard. | Adapt to Turtle battleground and teleport ownership. |
| Playerbots | `53ebb2d43`, `7f23556e2`, `4120aa9d3`, `7e6d0b26d`, `01635cdc2`, `b8fcbc4cf` | Cross-map summon/teleport, underwater path, and mid-teleport diagnostic paths contain additional safety work not present locally. | Treat as one staged teleport-safety series; reconcile with existing local transition generation and world/map-thread rules. |
| Playerbots | `7bcee961b` | Upstream replaces cached `Unit*` calculated values with `ObjectGuid`; the local tree still has 149 `Unit*` target-value references. | Separate high-risk lifetime-safety project with compile and gameplay staging; never bulk-apply blindly. |
| Turtle core | `0695ec09e` | `Creature::SelectLevel` can still truncate a positive template health value to zero. | Small core fix plus unit/startup validation. |
| Turtle core | `78db2249e` | Local Holy Strike/Mending Light work is only partial. It lacks the later coefficient, Blessed Strikes mask, target-map, and reduced self-heal corrections. | Adapt the complete spell contract and SQL together; class test. |
| Turtle core | `16e064e5a` | Moonwhisper quest/script/database update is absent. | Review as a content bundle and test migrations and quests on a copy of the world DB. |

### Priority 2 — targeted Classic/Turtle behaviour

These are plausible improvements, but each changes policy or gameplay and needs a focused test:

- `34d4adedf` — prevent paladins overwriting their own blessing.
- `37bf15e79`, `d4dda549e`, `5d9a61cd7`, `00e8f7318` — preserve long-cooldown boosts for instance bosses, including the null-spell guard.
- `9b1049f45` — allow conjured consumables in bot trades.
- `65d88a4c5`, `8b35c3196`, `9e780d0cf` — warrior Thunder Clap threat and non-protection Sunder policy.
- `a1b03eda1` — Thunder Bluff mesa elevator geometry.
- `bc67a2ebd`, `8aab5e200` — explicit raid-target crowd-control policy instead of assuming moon.
- `24c33023b`, `99e6f15eb` — remove apostrophes from Onyxia strategy keys that are persisted through SQL.
- `0bd44d886` — prioritise quest NPCs that are also the travel target.
- `d3664fd82` — retain portal nodes during travel graph cropping.
- `5d45ec633`, `07460fa33` — use 3D distance and reachability for enemy-player selection.
- `eda0c9e3a` — combat-stance positioning. This is high impact and should be isolated because it replaces existing behind/chase behaviour.

The historical revert `da1bb1f7b` has no independent runtime value; it is accounted for as part of the Thunder Clap series that was reapplied in `65d88a4c5`.

### Optional core surfaces

- Battleground world-thread queue series: `7e67f727d`, `44c236ba5`, `e228a5a82`. The local Playerbot integration already uses queue messagers in several paths, but it does not contain this canonical-core mailbox implementation. Port only after a battleground ownership review.
- Module API hooks: `07be1a329`, `b9219ea5f`, `35d62bf20`, `2beb53e0a`, `662d2e06d`. Current Playerbots use the consolidated `PlayerScript::OnChatCommand` seam. These hooks are useful only for broader module API parity or a future migration.
- Custom boss broadcast data cleanup: `7c599199c`. Review against the deployed Turtle database before importing SQL.
- SOAP series: `12f9d3e55`, `e04f4959c`, `c90a3c03e`, `08abac0ff`, `5d7948633`, `b1828ea0c`, `010cdb6d5`. The source tree contains optional SOAP code, but production Docker deliberately builds with SOAP disabled. Re-audit the complete hardened series only if SOAP is enabled.

## Turtle core ledger — all 55 commits

| Disposition | Commits | Audit conclusion |
|---|---|---|
| Superseded/incompatible Playerbot architecture (24) | `f9228f574`, `2128f7a36`, `2c520c71e`, `d6db86e54`, `65ada7003`, `51dda71ef`, `c645bcbb1`, `7efe82e09`, `f62cd95b6`, `8037fc8cc`, `40743ded4`, `e63161c2d`, `0addf4c92`, `c025c6818`, `aefca4b5e`, `7bf5ac260`, `fe8a2203c`, `d2a6e3f3a`, `779e074cf`, `6c921ae72`, `859b7ef08`, `36552c87c`, `0cbab007a`, `f8f33ea68` | Canonical core removed legacy PlayerBots and introduced generic headless/module lifecycle primitives. This fork deliberately retains integrated `mod-playerbots` with adapted hooks and ownership. These commits are migration reference material, not a cherry-pick queue. |
| Present/equivalent (4) | `9a765d340`, `67478ec5e`, `4fa52bad6`, `a686bc5f2` | Revision header, quoted `groups`/`rank`, formatting fixes, and Lower Karazhan data are already present through local equivalents/adaptations. |
| Candidate: battleground ownership (3) | `7e67f727d`, `44c236ba5`, `e228a5a82` | Potentially useful, but canonical mailbox types do not match the current bot queue integration. Requires contract review. |
| Candidate: module APIs (5) | `07be1a329`, `b9219ea5f`, `35d62bf20`, `2beb53e0a`, `662d2e06d` | Optional API parity; not required by the current consolidated Playerbot hooks. |
| Candidate: content/data (1) | `7c599199c` | Validate custom boss broadcast rows against the deployed database first. |
| Candidate: gameplay/correctness (3) | `16e064e5a`, `78db2249e`, `0695ec09e` | Genuine missing Classic/Turtle changes; queue separately as described above. |
| Optional SOAP feature (7) | `12f9d3e55`, `e04f4959c`, `c90a3c03e`, `08abac0ff`, `5d7948633`, `b1828ea0c`, `010cdb6d5` | Do not merge piecemeal while SOAP remains disabled. |
| Not runtime applicable to Linux deployment (8) | `3d995fd61`, `2414a67da`, `55cb75af0`, `23bf1fe5a`, `0fba5ae5f`, `1e1f11976`, `d886113c2`, `658fe5559` | Windows dependency/build refinements or documentation. Useful references, not missing Linux runtime behaviour. |

## CMaNGOS Playerbots ledger — all 80 commits

| Disposition | Commits | Audit conclusion |
|---|---|---|
| Accepted in the tested sync (5) | `266268c6d`, `ac013df69`, `2d5d62496`, `0b3e77f5d`, `a2ae599ef` | Adapted in source commit `2251f805d`, accepted by gameplay testing, and merged in `c6f6417eb`. |
| Present/equivalent (2) | `4cf72baeb`, `da312a9fe` | Local hostile-loot filtering is broader and predates upstream; local AhBot already gates update work on its loaded/enabled settings. |
| Correctness/lifetime candidates (12) | `6ad783c95`, `89a4e5aeb`, `7bcee961b`, `53ebb2d43`, `7f23556e2`, `4120aa9d3`, `7e6d0b26d`, `01635cdc2`, `b8fcbc4cf`, `8ef9193d9`, `e4b588836`, `b39956194` | Applicable wholly or partly. Stage the teleport and ObjectGuid work; take the small quest/resurrection fixes first. |
| Behaviour candidates (20) | `34d4adedf`, `37bf15e79`, `d4dda549e`, `5d9a61cd7`, `9b1049f45`, `00e8f7318`, `da1bb1f7b`, `65d88a4c5`, `8b35c3196`, `a1b03eda1`, `9e780d0cf`, `bc67a2ebd`, `24c33023b`, `8aab5e200`, `99e6f15eb`, `0bd44d886`, `d3664fd82`, `5d45ec633`, `07460fa33`, `eda0c9e3a` | Classic-compatible or potentially useful, but changes gameplay/policy and needs focused testing. The revert is historical context for the reapplied Thunder Clap change. |
| Diagnostic/admin optional (14) | `084a13687`, `46cec841d`, `ddbbfedad`, `1a5af653c`, `13523ae45`, `8f2df5ab1`, `f01fcb339`, `a541d244a`, `f299b8ba5`, `fe6f4b317`, `1ae52c413`, `d685f18f4`, `dcb2e15e2`, `54f2209ea` | Useful for debugging or administration, but not missing normal gameplay. Import only with a concrete operational need. |
| Test-only (21) | `12f75e6e8`, `449e6c1ae`, `d9f5bf153`, `89e2f06e5`, `526c36395`, `aca6fd5c6`, `46d4d2e85`, `1c326a5e8`, `59d2de1c7`, `e73783ac3`, `6f65246fe`, `f3dc330f9`, `27bf3f2df`, `612f15b1b`, `c1193bcb4`, `f4500093c`, `a5c7804a9`, `4532d6643`, `689eb574b`, `b60d39d12`, `14ccaa25b` | Not production runtime changes. Use relevant cases as designs for local regression tests while porting associated fixes. |
| Expansion-specific (4) | `b36cec53f`, `9bf2e4830`, `87dbe1022`, `8173334af` | WotLK/TBC-only; reject for the Classic/Turtle build. |
| Packaging/docs optional (2) | `fca6c42ae`, `1bafc2131` | Upstream AhBot uses a different config packaging model; the other commit is documentation only. |

## Verified local evidence

- The accepted branch was built successfully as image `pjw345/tortoise-docker:upstream-sync-2251f80` and passed the user's combat, attack, follow, and loot gameplay assessment. The observed post-combat loot delay and bag distribution/upgrade issue are separate follow-up defects.
- Local SQL already quotes the `groups` table and `rank` column, and contains the Lower Karazhan data update.
- The local hostile scan already removes units the bot is not actually hostile toward, covering `4cf72baeb`.
- The local quest auto-reward path still dereferences an empty set, confirming `8ef9193d9` is missing.
- The local resurrect packet handler still uses the two-argument registration, confirming `e4b588836` is missing.
- The local engine has no external-event retention/release map, confirming `b39956194` is missing.
- The local RTI crowd-control default is still `moon`, Onyxia strategy keys still contain apostrophes, portal nodes can still be cropped, conjured bot trades are still rejected, and enemy-player selection lacks the later reachability/3D-distance changes.
- The local `Creature::SelectLevel` path lacks the one-health clamp.
- The local Holy Strike/Mending Light implementation predates and does not contain the full `78db2249e` correction.

## Validation performed

- Enumerated all 55 core and 80 Playerbot non-merge commits in the fixed ranges.
- Compared patch IDs and reverse applicability where useful.
- Inspected every changed-file set and the production code for all correctness and behaviour candidates.
- Accounted for every enumerated hash in the two ledgers above.
- Documentation-only change: no server binary was rebuilt for this audit report.

## Closure criterion

The upstream snapshot can be called fully assessed now, but it should only be called fully integrated after every item in Priority 1 and Priority 2 has one recorded outcome: **adapted and tested**, **deferred with a reason**, or **rejected as incompatible/policy-unwanted**. New upstream commits after the fixed tips require a new incremental audit rather than reopening this ledger.

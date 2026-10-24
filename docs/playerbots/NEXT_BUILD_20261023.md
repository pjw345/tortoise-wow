# Next build preparation — 23 October 2026

User requested source coding while playing, explicitly **no compilation yet**.
Source branch: `codex/session-followup-fixes`, based on deployed stack `0e22ec9b`.
All changes below are uncompiled and unaccepted in gameplay.

## Prepared source

- #58: share the native repeatable gathering chest LOS exemption with Classic bot admission. Retain every other gate and the existing bounded repeat/depletion lifecycle.
- #66: private buyback success message with actual item/quantity only after confirmed native storage.
- #67: higher runed enchanting rod substitutes for lower rod in tool-presence checks only; no consumed-reagent substitution.
- Production helper/extracted-method regression fixtures under `tests/session-followups`; compilation and execution deferred.

Before the next build, review the diff and compile those fixtures and affected native server/variant targets. Then verify repeat mining on multiple veins, ordinary chest obstruction safety, successful/failed/multiple buybacks, and enchanting normal/trade casting with only the higher rod. Source extraction and whitespace checks are not compilation or runtime validation.

## Separate follow-ups

- Stonewatch's scoped installed data repair is live-tested as improved. The permanent extractor collision fix is **not coded** here. The existing assembler deduplicates solely by numeric placement ID and can alias a WMO with an M2. A correct fix must reserve IDs, maintain placement-kind identity across every repeated tile reference, preserve native spatial-tree contracts, and test conflicting namespaces/overlays before fresh extraction. Do not edit the running data again as part of this source draft.
- Bobbing Apples: configured item/source exclusions already exist, but Cria was observed interacting again. The bypass path remains unproven; capture per-bot discovery/selected action/source identity before editing shared loot or RPG logic.
- Sell Junk: installed WarbandQuestHelper now sends `s gray` instead of `s vendor`; source edits are in the separate `warband-quest-helper` checkout. This is a client button restriction, not a change to generic server vendor-item classification. Reloading the client UI is required to use the changed button.

No image build, deployment, restart, database mutation or GitHub merge is part of this preparation.

## Build authorization — 24 October 2026

The user authorized compilation with game services stopped. GCC 11.4 focused session-followup and existing native-loot-followup CTest suites both pass (one test each). A missing test-context mock API was corrected before the successful run. Full server image build follows from committed source; activation and gameplay acceptance are separate.

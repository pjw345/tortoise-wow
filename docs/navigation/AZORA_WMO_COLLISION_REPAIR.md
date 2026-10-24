# Tower of Azora: WMO filename collision repair (21 October 2026)

Issue #62. This repair is built on the party-progress branch (PR #61), preserving the existing playtest changes.

## Demonstrated cause

The client ADT `World\Maps\Azeroth\Azeroth_33_49.adt` names the original asset `world\wmo\azeroth\buildings\magetower\magetower.wmo`. The client also contains `World\custom\Dalaran\magetower\magetower.wmo`. These are different roots with the same basename. The former root resolves from `wmo.MPQ`; the latter resolves from `patch-3.mpq`.

The extractor previously flattened WMO paths with `GetPlainName` and skipped an output if that basename already existed. Therefore different assets could silently share one collision file. ADT, WDT/global and game-object metadata also stored plain WMO names.

The deployed `Magetower.wmo.vmo` was byte-identical to `Kt_Magetower.wmo.vmo` (SHA-256 `6adc5b551079e0c51c0b84dc7cf53431288d2671811e22f99f75b7af415acfe7`, 994,676 bytes). Reassembling the original client tower through the native extractor and `TileAssembler::convertRawFile` produces a different 329,600-byte collision model, SHA-256 `cff9bfa4cff797f655b67a35ce229923bfbb2d8ad103bc76f93359e9e847dfe0`.

Changing only the selected tower's binding to that original model, then regenerating navigation with unchanged default generator settings, changes the outside-to-Theocritus native path from partial to complete. Default regeneration alone, rasterization-limit changes, coarse resolution, quick-mode diagnosis and a transition-climb experiment all leave that route partial. No settings experiment is deployed.

## Source correction and compatibility

`WmoOutputName.h` creates a deterministic identity from the complete case-folded MPQ source path (slash direction is normalized). It retains the legacy-spelled basename as a suffix for existing model-name substring rules. Root extraction, ADT references, WDT/global references and game-object WMO records all use this one function. M2 naming is unchanged. The existing Zeppelin geometry adjustment still keys off the original basename.

The format of raw/final collision and navigation data is unchanged. Native WMO collision selection, embedded M2 geometry, material flags, root/group IDs, native assembly and runtime loading remain responsible for their existing semantics. No client MPQ, SQL, runtime collision check, teleport fallback or movement policy is changed.

A **fresh complete extraction** with this extractor uses new WMO filenames throughout. Rebuild the corresponding metadata and navigation together in an empty staging directory; do not mix newly named models with legacy metadata. Existing deployed data is not automatically renamed. Full world/dungeon extraction acceptance has not been performed.

## Tools and focused repair procedure

Build the existing extractor/generator and the bounded offline helpers with `USE_EXTRACTORS=ON`:

```sh
cmake --build build --target vmapextractor vmap_model_tool MoveMapGen mmap_path_check -j2
```

`vmapextractor -d INPUT --wmo 'World\wmo\Azeroth\Buildings\Magetower\magetower.wmo'` exports only the requested root and its groups to the normal `./Buildings` work directory, through existing archive loading and WMO conversion. Use the verified matching client archive overlay. This particular reconstruction used the original `wmo.MPQ`, `model.MPQ` and `patch.MPQ`; the latter supplies LiquidType.dbc. Verify resolution again for other clients rather than assuming that archive subset is always sufficient.

The resulting identity is `wmo_e5a88432c8aa603d_Magetower.wmo`. Assemble it using the native pipeline:

```sh
vmap_model_tool --assemble Buildings STAGED_VMAPS wmo_e5a88432c8aa603d_Magetower.wmo
```

For the existing deployment, inspect all map-0 tile references to native spawn **71434**, tree index **26386**. Both `000_33_49.vmtile` and `000_33_50.vmtile` refer to it. Rebind that exact spawn in both tiles in staging:

```sh
vmap_model_tool --inspect ORIGINAL_TILE
vmap_model_tool --rebind ORIGINAL_TILE NEW_TILE 71434 Magetower.wmo wmo_e5a88432c8aa603d_Magetower.wmo
```

The helper reuses `ModelSpawn`'s native reader/writer. It requires exactly one matching spawn/name and a new output path. All unrelated records are copied byte-for-byte, including records whose native reader applies an in-memory no-LOS compatibility flag. Existing transforms, bounds, flags and BIH tree indices are retained. No vmtree rebuild is needed for this original-asset correction; the map spatial tree and ADT transforms/bounds remain the original ones. Verify all references rather than changing only the first tile loaded.

With `maps/`, the staged `vmaps/` and an empty `mmaps/` available in the generation directory:

```sh
MoveMapGen 0 --tile 33,49 --force --silent
MoveMapGen 0 --tile 33,50 --force --silent
mmap_path_check ORIGINAL_000.mmap STAGED_MMAPS -9574.13 -738.215 64.0125 -9553.08 -728.152 99.2522
```

Preserve the deployed `000.mmap`: this rebuild's generated map origin differs by floating-point rounding, and all checks explicitly use the existing deployed map parameters. Replace only the two `.mmtile` files after verification, alongside the new uniquely named `.vmo` and the two changed `.vmtile` bindings. Preserve the existing generic `Magetower.wmo.vmo` and custom models. Back up all four replaced files, record all five new-file hashes, and provide rollback before installation. Stop game services before builds or replacing loaded data.

`mmap_path_check` uses actual Detour nearest-poly and path queries with native ground/steep flags. A partial path, missing endpoint, exhausted node/buffer limit or failure is unsuccessful. It can load a single tile or all matching tiles from a directory. Its displayed funnel corners are not a reproduction of the runtime smoothed movement spline or a gameplay acceptance test.

## Verification and remaining acceptance

- All three changed tools compile against native dependencies.
- Production output-identity tests distinguish same-basename assets, normalize case/slashes and preserve legacy basename substrings.
- Nine production native model-tool tests cover byte-exact identity/targeted rebinding, unrelated no-LOS records, malformed/truncated input, wrong/missing/duplicate identities, unsafe names and existing-output refusal.
- Both real collision-tile replacements equal the original bytes with only the selected filename length/name substituted.
- Eighteen native paths between the outside and successive stair/floor positions succeed in both directions with the original map parameters and both staged navigation tiles.
- The original data and five generator-only variants fail the same primary path (six negative controls).
- Native collision probes confirm nearby floor support at sampled mesh edges. Two simplified funnel segments touch collision edges; actual runtime smoothing/formation clearance still needs gameplay testing. Do not describe the offline checks as proof that every movement corner is fixed.
- The original custom model stays unchanged.

The user must still test following into the tower, all the way up, back down and outside, including different formation offsets and temporary obstruction recovery. Keep #62 open until that acceptance. Stonewatch and Stormwind require separate source-asset/navigation checks; this repair does not establish their cause.

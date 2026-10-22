# Stormwind: missing WMO tile references (22 October 2026)

Issue #21. Source branch is based on PR #64, retaining the existing playerbot and party-quest changes.

## Evidence and native contract

Alon's captured map-0 position was `(-8437.65, 579.799, 94.199)`; Alin's was `(-8394.79, 575.651, 91.2677)`. With the deployed map parameters and native ground/steep query flags, neither endpoint was on the navigation mesh. A lower terrain control at `(-8448, 576, 59.5268)` was on the mesh. The native collision manager, loading only tile `30,47`, found no floor near either character's recorded height.

The existing Stormwind placement (spawn **10047**, BIH index **745**, model `Stormwind.wmo`) has horizontal internal bounds `(24986.884765625, 15603.357421875)` through `(26474.984375, 17091.708984375)`. They overlap 16 native tiles: filename X 29–32, filename Y 46–49. Only six deployed tiles reference that placement: `(29,48)`, `(29,49)`, `(30,48)`, `(30,49)`, `(31,49)` and `(32,49)`. The other ten omit it, including `30,47`.

Read-only client inspection found the same placement in lower-overlay ADTs, while `patch-8.mpq` ADTs `30,47`, `31,47` and `31,48` omit it. This is a placement-reference coverage problem; it does not demonstrate a Stormwind model-identity collision. The existing 37,438,934-byte model has SHA-256 `d9a1da69f2e0d0459a59f8a82191402b78a7df51de470112e0dfb8593a0a4277` and remains unchanged.

`TileAssembler::readMapSpawns` deduplicates placement identities but previously retained only the ADTs' explicit tile references. `StaticMapTree` loads collision instances through those per-tile records, and MoveMapGen likewise loads the tile's collision references. The assembler now completes bounded, non-global WMO references from their recorded horizontal bounds. Tile filename axes are reversed relative to internal coordinates. Existing references are retained; added `(tile,spawn)` pairs are deduplicated. M2 placement handling, WDT/global maps, transforms, BIH indices, model contents, collision materials, liquids and native formats are unchanged. Invalid/nonfinite or wholly out-of-map bounds generate no added references.

This changes native extraction metadata, not follow policy. No guessed bridge, movement shortcut, teleport, collision bypass or arbitrary Z adjustment is introduced. Other terrain and instance models still require their own evidence and gameplay acceptance.

## Scoped existing-data repair

Fresh extraction uses the corrected assembler. For the existing deployment, the offline native model tool can copy a verified existing WMO reference into a missing overlapping tile in the **same map bundle**:

```sh
vmap_model_tool --include VMAPS/000_31_49.vmtile VMAPS/000_30_47.vmtile NEW_OUTPUT 10047
```

Source and target must be in the same directory and map namespace. The tool requires exactly one matching bounded non-global WMO in the source, no conflicting ID/tree index in the target, overlapping placement bounds, valid native input and a new output path. Both original files are retained. Existing target records and the appended source record are copied byte-for-byte, preserving the original tree index and flags (including native reader compatibility side effects). The caller must verify that all inputs belong to the same map-tree/model bundle; filenames alone do not prove provenance.

For Stormwind, restore only the ten missing references listed above, then regenerate their corresponding navigation tiles with unchanged generator defaults. Keep models, `000.vmtree`, the deployed `000.mmap`, all six existing reference tiles and the Azora repair unchanged. Generate in isolated staging with original terrain/collision mounted read-only, never through writable hardlinks into production. Stop game services for builds and replacement. Back up every replaced tile, record before/after hashes and retain a rollback manifest before installation.

Example tile generation and native route test:

```sh
MoveMapGen 0 --tile 30,47 --force --silent
mmap_path_check ORIGINAL/000.mmap STAGED/mmaps -8437.65 579.799 94.199 -8394.79 575.651 91.2677
```

Check generated native data and route results rather than relying only on MoveMapGen's process exit status. This fork's single-tile path ends with process status 1 even after successfully writing the tile. Preserve the deployed map parameters: generator map-origin rounding can differ.

## Validation and acceptance

The first repaired tile changes native collision floors from missing to `94.1993` and `91.2928` at the recorded endpoints. Default navigation regeneration changes the same route from missing endpoints to a complete 12-polygon corridor, with the original map parameters. Direct line of sight between endpoints is obstructed, so a route around intervening collision remains necessary.

Regression coverage exercises coverage axes/edges/clipping/nonfinite bounds, production assembly across multiple tiles without duplicate references, preserved global/M2 handling, and byte-exact scoped reference inclusion with malformed/duplicate/out-of-bounds/conflicting inputs and overwrite refusal. Run:

```sh
cmake -S tests/vmap-model-identity -B coverage-tests
cmake --build coverage-tests
ctest --test-dir coverage-tests --output-on-failure
python3 tests/vmap-model-identity/test_model_tool.py --tool BUILD/tools/vmap_assembler/vmap_model_tool -v
python3 tests/vmap-model-identity/test_tile_coverage.py --assembler BUILD/tools/vmap_assembler/vmap_assembler -v
```

Offline floors/connectivity and a compiled server do not certify runtime formation offsets, every city street, dungeon zoning or live following. Keep #21 open for the Stormwind entrance/bridges, Trade District, Cathedral Square, Dwarven District and Deeprun Tram approach playtest. Preserve the original Azora upper-floor acceptance separately under #62.

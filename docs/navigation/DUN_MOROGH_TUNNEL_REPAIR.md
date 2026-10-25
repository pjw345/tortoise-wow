# Dun Morogh tunnel: M2/WMO placement-ID collision

Issue #78. Offline investigation and candidate repair, 25 October 2026.

## Demonstrated cause

Alin entered a tunnel at map 0, world (-5070.124023, -1459.865723,
558.837708), approximately Dun Morogh 66.23,36.34. All four controlled bots
stopped outside. Installed native Detour data has start polygons for the bots
but no endpoint polygon at Alin; native collision finds no model floor there.
GPS instead reports terrain at 637.691589.

Client patch-9.mpq places `World\Wmo\Dungeon\Md_Dwarven Tunnels\MediumTunnel.wmo`
UID 5525455 in ADTs 34,41 and 35,41. Native internal placement coordinates are
(22105.4375,18669.910156,512.125793), rotation (0,-167.833191,0), scale 1.
Its bounds contain the captured player point. The installed Mediumtunnel model
contains all ten client group triangle sets (including the additional triangles
retained by large-mode extraction); the model itself is available and unchanged.

Installed native metadata instead binds UID 5525455 in both tunnel tiles to
`Westfallshed.m2`, tree index 118122, at distant internal coordinates
(19297.033203,14992.777344,32.363369), also referenced in tile 28,36.
`TileAssembler::readMapSpawns` previously keyed both MDDF/M2 and MODF/WMO
placements solely by numeric ID. The first model won, and tile references for
the other type inherited its geometry and spatial-tree index. Regenerating
from the same inputs with that assembler repeats the omission.

## Source correction

Assembly identity is now `(native ID, is M2)`, shared by unique placements,
per-tile references, bounded WMO coverage and spatial-tree index lookup.
Both placements survive independently. Duplicate records of the same type and
ID retain the existing first-record semantics. Other flag bits do not create
new identities. Native serialized IDs, file formats, map partitioning, global
WMO handling, runtime loading/unloading, model-name rules and movement policy
are unchanged. Runtime StaticMapTree loads instances by tree index; equal
numeric IDs at different indices do not alias.

A fresh complete assembly can assign different spatial-tree indices. Generate
and distribute its matching vmtree/vmtile bundle together; do not mix it with
old tile references. Rebuilding navigation alone cannot restore this placement.
This fix covers demonstrated cross-type ID collisions; same-type conflicting
IDs retain existing semantics and require separate investigation.

## Focused staged data experiment

Evidence and scripts are retained outside Git under
`/home/paul/development/live-follow-20261025/`. Game client assets and generated
collision/navigation binaries are not committed.

The captured installed map-0 metadata contains 132456 unique, contiguous tree
indices with consistent duplicate records. Rebuilding the original BIH from
these native ModelSpawn bounds, in existing index order, reproduces the installed
000.vmtree byte-for-byte. This negative control must pass before using the
candidate. The diagnostic then appends the actual client tunnel placement at
index 132456 using the existing native BIH builder and file writer. It adds the
placement to staged tiles 34,41 and 35,41 without altering any original record,
model or tree index. The existing shed remains preserved at its original index.

The staged native VMap query finds floor 558.837 at the captured player point.
Two affected navigation tiles are regenerated with unchanged MoveMapGen
defaults. Always use the deployed 000.mmap for route validation and installation;
do not replace map-wide parameters with the generator's newly written file.
The scope is one vmtree, two vmtile references and two navigation tiles.
This diagnostic staging workflow is not a general-purpose migration utility.

## Verification and remaining acceptance

- Corrected production assembler compiles against native dependencies.
- Production fixture exercises colliding M2/WMO IDs in both input orders,
  repeated WMO references and unchanged first-record handling within one type.
  The original assembler fails both collision-order negative controls.
- Existing native bounded-coverage fixture and 12 model-tool regressions pass.
- Native baseline tree reproduction is byte-identical; client model geometry
  and restored captured collision height are checked separately from navigation.
- Record generation results and native forward/reverse route checks in issue
  #78 before considering installation. Successful tool output is not live
  party-following acceptance.

No live map files or services have been changed by this work. Install only a
validated, hash-manifested bundle with a stopped world server, a backup and a
rollback path. Then test normal party following through the tunnel in both
directions, including formation offsets and combat interruption/recovery.
Keep issue #78 open until live acceptance.

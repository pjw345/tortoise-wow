Configure/build/ctest this directory with CMake and Python 3. C++17 is required.

extract.py compiles the actual production gear destination/comparison/classification,
upgrade scan and native dispatch wrapper; the complete buff-consumable implementation;
and the strategy reset method. Engine/world/inventory services are small contract fakes.
Gear tests keep a cache constant across the pass to reproduce the original downgrade.
Native handler failure is represented without moving the equipment and must not be
reported as success. Buff tests verify delegates to the native spell-group and
stronger-aura gates, not an independent stacking implementation.

Coverage includes paired-slot ordering/eligibility, suffix upgrades, same-tick scans,
repeat scans, forced equipment, gray armor, usability, combined equipped hand score,
failed swap reporting, empty/non-empty bag replacement, final roll feedback, classic regeneration/scroll effects, food independence,
caster Strength exclusion, lower tiers, cooldowns, active ranks, rejected-candidate
fallback, self-targeting, owned inventory, and legacy saved strategy activation scope.

Limits: these tests do not run a complete server, load production SQL/DBC content,
measure gameplay or prove every possible bag-weapon combination optimal. The full
native image must compile and actual party behavior must be tested separately.

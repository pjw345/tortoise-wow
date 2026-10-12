# Native spell capability fallback regression

Tests compile the actual CastSpellAction::isUseful/isPossible methods extracted
from production source. Fixtures supply core interfaces; this is not a complete
server or action-engine simulation. Missing capabilities must remain useful but
impossible so Engine's existing alternatives branch can run. Cases cover
unresolved/unlearned/missing-data spells, learned spells, mount, target/map/
vehicle usefulness, policy rejection and range rejection.

Run CMake configure/build and CTest for this directory. The tests fail against
the previous usefulness-stage capability guard. Native priest fallback graph:
greater heal on party -> heal on party -> lesser heal on party.

# Follow movement inspection regression

Run `cmake -S tests/playerbot-follow -B build/follow-tests`, build that directory,
then run `ctest --test-dir build/follow-tests --output-on-failure`.

The suite executes the native targeted-generator state/accessors, facade
inspection methods and final follow-dispatch guard. Motion-stack and follower
services are deterministic stand-ins. Player and creature follow/chase variants,
combat-victim disagreement, repeated requests, rounding, angle wrap, target and
formation changes, missing/invalidated targets and unrelated motion are covered.
Both admission callers are checked to use the same comparison.

This does not exercise navmesh generation, full spline execution, water/taxi/
transport movement, live city navigation or loot interactions. No production
server, database or container is used. The Stormwind route still needs a client
test after a normal build/deployment.

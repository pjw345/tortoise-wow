# Native movement compatibility regression

cmake -S tests/playerbot-native-movement -B /path/to/checks
cmake --build /path/to/checks -j 1
ctest --test-dir /path/to/checks --output-on-failure

The captured moving-spline/zero-unit-state case must fail with the old ZERO
facade. See docs/playerbots/NATIVE_MOVEMENT_STATE.md for extracted boundaries
and live acceptance. Other macro branches are predicate fixtures, not complete
expansion-server builds.

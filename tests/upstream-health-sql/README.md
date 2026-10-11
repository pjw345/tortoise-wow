# Focused upstream health / GM SQL regressions

Run the native health cases without compiling the complete server:

```sh
cmake -S tests/upstream-health-sql -B build/health-tests
cmake --build build/health-tests --config Release
ctest --test-dir build/health-tests -C Release --output-on-failure
```

CMake extracts the production `Creature::SelectLevel` health block. The
harness supplies the template, health-rate and setter boundaries rather than
copying the fix into the test. Cases cover truncation, intentionally zero
templates, interpolation, reversed template bounds, normal/increased rates,
spawn-health percentages and the existing April Fools override.

These checks do not load creature templates from a real world DB, exercise
pet initialization, kill/respawn a live creature or replace a full core build.

`test_gm_sql.py` extracts the production account-list query and runs it against
the disposable MySQL 8.4 service in the GitHub workflow. It checks positive-rank
filtering and reserved-identifier syntax. `MYSQL_CONTAINER_ID` identifies only
that test service. Never point it at a production database: the fixture creates
its own `regression` database and account table.

The source-audit CI job invokes `audit-core-contracts.ps1` without
`-RefreshDatabase`; it reads the committed audit snapshots and source only.

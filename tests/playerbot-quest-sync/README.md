# Playerbot quest-sync regressions

Configure this directory as a standalone CMake project, build and run CTest.
`extract.py` compiles the production `QuestSync.cpp` adapter with host API fakes;
`QuestSyncProtocol.h` is included directly. No game server or database is needed.

The AddOn and client suite are maintained separately in
[pfQuest-partybots](https://github.com/pjw345/pfQuest-partybots).
Run `QuestSyncTest /path/to/native-wire.hex` to save the adapter's actual native
packets as hex lines, then run from the companion checkout:

```sh
python -m pip install -r tests/requirements.txt
python tests/test_client.py --wire /path/to/native-wire.hex -v
```

`--deps <directory>` can select a local Lupa dependency directory.
`--lua50 <executable>` repeats the complete cases on Lua 5.0.3.
`--pfquest-tracker <tracker.lua>` extracts the installed pfQuest's unmodified
ButtonEvent and DoLayout implementations; only their host APIs and input values
are faked. No pfQuest source is distributed in either repository.

The companion's default synthetic fixture permits independent client testing.
Regenerate and pass the current native fixture when changing the server protocol
or adapter; the checked-in companion fixture alone does not establish compatibility
with a modified server. Coordinate protocol-version changes across repositories.

These focused checks do not replace the full native build, actual client AddOn
delivery/rendering or live quest-progress acceptance.

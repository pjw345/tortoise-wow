# Turtle enchanting rod client compatibility patch

Source tooling for issue #67. This creates a client-specific candidate, not a
replacement WoW client. It requires the corresponding higher-rod tool validation
on the private server. The generated
game data and archives stay outside Git.

## Design and compatibility

Wrath 3.3.5 implements tool categories and masks. The current Turtle client uses
the older 173-field Spell.dbc format and has no such category fields. Copying
Wrath's 234-field file or adding unsupported fields would be incompatible.

The builder reads the exact client's MPQ overlay and Enchanting skill-line
associations. It clears only the two client exact-item tool fields when they
name one of the five server-supported runed rods. All other fields, spells,
strings, native formats, consumed reagents/counts, non-rod tools, skill and
target requirements remain unchanged. The existing server HasRequiredTool
hierarchy remains authoritative. No executable, DLL, native crafting API,
server data or original client archive is modified.

The companion Warband Enchanting Rods addon restores the recipe's rod display
as "Runed Copper Rod (or higher)", checks carried bags before DoCraft, and
then delegates to the original native function with its original arguments.
It preserves other tool requirements and ordinary craft calls. Rods in bank
bags do not count. If native exact-rod focus still appears, the addon leaves
the original UI/crafting behavior intact, rather than advertising an active
patch. `/werods` reports the selected recipe's patch status.

The MPQ is required for client cast initiation; the addon provides the display
and local precheck. Without the addon, server validation still rejects missing
or insufficient rods, but the client no longer displays those tool requirements.
Direct spell casting continues through normal server checks. The addon never
replaces target selection or non-traded trade-slot handling.

## Build

Requires Python with mpyq and the official StormLib native library. This candidate
uses StormLib v9.25, commit 28c9b4be3f23c6b3a5ff55cacac7dbe5b9cdc4fc.

```text
python build_patch.py --client "D:/TurtleWoW Client with Addons" --output OUTSIDE_CLIENT
python pack_mpq.py --stormlib /path/to/libstorm.so --candidate OUTSIDE_CLIENT
```

The output path must not exist and must be outside the installed client. The
builder validates the field layouts and Minor Striking contract, rejects
ambiguous names, and audits every output row. Header, string-table and all
non-rod-field bytes must remain identical. The packager creates a classic MPQ
with zlib compression and verifies the extracted file using StormLib itself.

## Tests

```text
python -m unittest discover -s . -p test_patch.py -v
lua50 test_rods.lua .
```

Python fixtures cover consumed reagents, unrelated tools/spells, multiple tools,
bad layouts/truncation/duplicate IDs and name/rank ambiguity. Lua 5.0 exercises
all 25 rod-tier combinations, missing/bank-only tools, native argument/return
forwarding, other crafting, tool display and unpatched-client fallback.

## Candidate status and acceptance

The generated candidate is based on the user's current patch-9.mpq Spell.dbc
and SkillLineAbility.dbc. It preserves 27916 spell rows and changes 172
Enchanting recipes, with 344 differing bytes. Original Spell.dbc SHA-256:
fbc0a5a2e51ffe1110d0d85e9a1f885a68c4b3b2695b7e83bfc26c478af3ff6a.

Offline checks do not establish client startup, MPQ precedence or successful
enchanting. Test a separate compatible client copy before adopting it. Keep
issue #67 open until startup, all tier directions, no/bank-only rod rejection,
normal and trade enchants, reagent consumption and removal are verified live.

The release includes a hash-checking Install.ps1, a manifest and installation
instructions. It refuses a running target client, existing destinations, changed
client archives and mismatched payloads. Removal moves only matching patch/addon
files into that client's Archive directory. Original MPQs remain untouched.

After any client update, remove this patch and regenerate it from the new client
before reusing it: a complete Spell.dbc override otherwise masks newer spell data.

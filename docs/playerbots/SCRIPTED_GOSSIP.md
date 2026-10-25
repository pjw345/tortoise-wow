# Explicit Talk for script-generated quest menus

Issue #83, 25 October 2026. Source correction for the next server build.
No runtime quest/item changes, deployment or live acceptance have been performed.

## Reproduction and native ownership

Running revision 18d7060b rejects explicit Talk before native hello when an NPC
has no gossip_menu_option rows. Maltimor Gartside (60858, npc_maltimor_gartside)
uses menu 41162 without database choices. His registered script creates the
conversation for incomplete quest 40529 with letter 60767. Four choices call
INFO_DEF+1 through +4. Only +4 consumes the letter, saves inventory and grants
the dummy 60381 credit. A letter must first have started the quest; possession
alone does not establish an active journal entry. All four bots carried the
letter in the read-only snapshot; online saved quest state can lag.

The chat strategy maps `talk` to GossipHelloAction and the existing quest-giver
action. Native HandleGossipHelloOpcode sets the session's current NPC identity,
validates interaction, handles talk-interrupt auras/movement and invokes ScriptMgr.
ScriptMgr owns legacy/registered/module/Lua hooks and database fallback. Native
selection resolves the current menu sender/action, repeats interaction checks,
clears the menu in script dispatch and invokes the selected script. These paths
already implement the required behavior and are retained, not duplicated.

## Correction

Database menu admission is now limited to autonomous `rpg action` gossip, where
its previous rule is preserved. Explicit owner Talk reaches the existing native
hello even for dynamically generated choices. It reports native text and numbered
options. Each explicit `talk N` submits one ordinary native selection; no quest
IDs, NPC IDs or automatic dialogue traversal are added to production code.

Vanilla numbered selections require the session's current gossip NPC to match
the target. Empty/final menus return before random selection or indexing; other
negative/out-of-bounds indices fail, and failed selections propagate failure
instead of claiming success and awarding generic TalkedToCreature credit.
The existing non-Vanilla packet layout is unchanged. The native fixture uses
the actually supported Turtle/Vanilla build; other expansion builds and live
module/Lua scripts are not separately compiled/exercised here.

## Use after a new server build is activated

Each bot needs the accepted quest and its own letter. If necessary, use the
actual item link with the existing `u` command to start it first. Stand within
normal interaction range and keep Maltimor selected. Whisper one bot, or send
to the party when all affected bots are eligible and in range:

```text
/p talk
/p talk 1
```

Wait for the bot's response between commands. Maltimor has one reply at each
stage: send `talk 1` four times in total after the initial `talk`. Then use the
existing Turn In action. For another NPC, read the numbered replies and choose
the intended option instead of assuming option 1 is appropriate.

## Verification and limits

The test extracts the complete production GossipHelloAction implementation,
actual native hello/select opcode handlers and both real Maltimor dialogue
functions. Fixture services supply interaction state, menus and ScriptMgr dispatch.
Four independent character instances progress through all four real script
steps, with no early credit or letter consumption and no repeat final credit.
Controls exercise missing letters/unaccepted quests, empty menus, bad indices,
wrong gossip identity, target packets, out-of-range interaction, database fallback
and retained idle admission. This is executable native-fragment coverage, not
a full server process, real database transaction or in-game acceptance.

Run:

```text
cmake -S tests/playerbot-scripted-gossip -B /path/to/checks
cmake --build /path/to/checks -j 1
ctest --test-dir /path/to/checks --output-on-failure
```

Keep #83 open until the full build and actual multi-bot conversation/turn-in
pass. No changes to Maltimor's quest script, costs, prerequisites, rewards,
automatic quest completion or online database state are made.

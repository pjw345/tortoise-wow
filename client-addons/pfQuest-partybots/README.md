# pfQuest Party Bots

Shows controlled party bots' live quest progress under pfQuest's tracked quests.
The existing pfQuest files, quest database, quest actions and key bindings are
not replaced. This extension requires the matching Playerbot server changes.

## Display

- Compact per-bot progress below each tracked quest, two names per line.
- Hover a quest to see each bot's state and collected/required objective counts.
- `Ready` means the bot's native quest status is complete but unrewarded.
  `Turned in` means the server records a previous reward and the quest is not
  currently in that bot's log. A repeatable quest accepted again shows its active
  state instead of being hidden by its earlier reward history.
- `Not in log` means the quest is absent from the current log. It does not mean
  never accepted or definitely turned in. Historical states are supplied for
  the player's current quest IDs included in the request.
- Item, creature/game-object or cast objective counters, exploration/event
  completion and required money are read from native bot state. Percentages
  aggregate those quantitative objectives; native `Ready` remains authoritative
  for requirements such as reputation that are not quantified in this view.
- Pending, offline, unavailable and stale data remain distinct. `*` marks stale
  or unavailable data. The tooltip shows the last response time and any error.
  The last complete snapshot survives partial replies, timeouts and reloads;
  reloaded data is stale until the server confirms it.

`Party quests` opens the full comparison from either the tracker or the normal
Quest Log (normally opened with L). It includes quests held only by bots and
keeps identically named quests separate by ID. The tracker shows up to four
party-only quests below its normal entries; the panel includes all current party
quests. Raid tooltips/panel include the full roster with member paging; compact
tracker lines show the first four other members.

## Installation and use

1. Install/build the matching server revision while game services are stopped.
   Installing this AddOn alone on an older server will show unavailable data.
2. Exit the client. Extract `pfQuest-partybots` into `Interface/AddOns`, beside
   `pfQuest` and `pfQuest-turtle`. Keep pfQuest enabled and enable this extension.
3. Restart the client and group with controlled bots. Data is requested for the
   current roster; no character names are configured in the AddOn.
4. Use `/pfqb` to show/hide the party panel and `/pfqb refresh` to request an
   update. Personal quest details, tracking, sharing, abandoning and the existing
   Warband Quest Helper buttons keep their normal behavior.

Ordinary polling is every 15 seconds per member, spaced by at least half a
second between requests. Manual/player-quest refreshes respect a six-second
per-member minimum. The server independently permits at most one queued request
and one response every five seconds per bot. A reply times out after 15 seconds;
freshness expires after 45 seconds. Real party members who are not bots controlled
by this player do not supply data and remain unavailable rather than empty.

## Integration and protocol

The extension uses the installed pfQuest 8.0.1 tracker's `ButtonEvent`,
`ButtonEnter` and `DoLayout` functions. It wraps those functions and preserves
their ordinary behavior. If the required surface is absent, the tracker hook
stays disabled with one notice; the comparison is still accessible using `/pfqb`.
No fake personal quests are inserted into pfQuest's quest log or map database.

Requests use the exact `PFQBOT` AddOn prefix on native party/raid chat. The module
consumes it before ordinary command parsing and conversation. Requests are checked
against the bot's real-player master and group, queued through native bot command
processing with the requester's GUID, then authorized again at capture time.
The recipient is looked up afresh, so a logout or master/group change cancels
the reply. Only the bot owner's quest state is traversed on its AI owner.

Replies use native `Player::SendAddonMessage`: local GUILD-type packets with the
recipient's identity, not a guild broadcast. The client accepts only its own
identity, an outstanding request token, a current roster member, and matching
version/hash/chunk metadata. It replaces a snapshot only after a complete valid
reply. Titles are hex-encoded data. No payload is executed and no human whispers
are parsed, filtered or hidden.

Protocol 1 request: `Q|1|token|bot|previousHashOr0|questIds` (at most 240 bytes,
25 queried IDs). A snapshot contains at most 50 quest rows: actual quest slots
plus the requested current-player history IDs. Each row is
`questId|state|level|hexTitle|objectives`, followed by a newline. States are
`I` in progress, `C` ready, `F` failed, `R` rewarded history, `N` absent.
An objective is `kind,index,entry,current,required`, separated by semicolons;
kinds are `I` item, `K` creature/cast, `G` game object/cast, `E` event, `M` money.

`S|1|token|bot|hash|part|total|payload` carries a snapshot in at most 192 chunks of
128 bytes. `U|1|token|bot|hash` confirms an unchanged snapshot. Oversize replies
return a bounded error. The FNV hash is a change identifier, not authentication;
native sender identity, owner/group checks and request tokens establish context.

## Validation and remaining live checks

The standalone C++ fixture compiles the production adapter and protocol using
native API fakes. It checks ownership, group change, logout, rate limits, packet
bounds, history versus active repeatable quests, and actual counters without
quest-map insertion. Its generated packets are consumed by the shipped Lua.

Client tests execute on Lua 5.0.3 and Lua 5.1 using Vanilla API fakes. Optional
tests load `ButtonEvent` and `DoLayout` directly from the installed pfQuest
tracker, checking repeated rendering, modes, bot-only rows, IDs, response
assembly, stale/empty/offline states, and objective tooltips. These are not
live client graphics or server-startup validation.

Before acceptance, build the full native server while stopped, install the
extension after exiting the client, then check an item objective, a kill/cast
objective, a bot-only quest, ready and rewarded states, roster changes, a missing
reply, and the ordinary pfQuest/Quest Log/helper controls in game. This feature
never accepts, rewards, abandons or changes a quest, writes to the database, or
changes equipment/combat/loot behavior.

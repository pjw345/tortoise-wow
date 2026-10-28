# Box formation

Select with `formation box`, then use normal `save ai` to persist the selection.
This is an opt-in server feature; no client/addon change or new default setting.
It is not available on the running image until a requested build and activation.

For the current melee-led party, the intended travel arrangement is:

```text
          direction of travel
                  ^
          master    melee
       caster  caster  caster
```

Native tanks/melee use the front row. Native ranged/healers share the rear row;
ranged/healer predicates take priority over assist strategy type bits. The
native tank-assist strategy advertises a tank bit even on casters, so IsTank
alone cannot assign a travel row. This uses roles, not
class or character-name lists. Members are sorted by GUID within each row so
unchanged membership/roles retain their slots even if group-reference order
changes. The human remains the anchor and is never moved by the formation.
A ranged/healer master anchors the rear row, with melee in front of that row.

Each bot's existing `range follow` is the column and row spacing. With common
spacing 2 yards, two front/three rear members and the master first in GUID
order, the melee companion is 2 yards to the master's right; the rear row is 2 yards behind and centered between the
front pair. Its outer slot is about 3.6 yards from the master. Different per-bot
range settings scale that bot's offset and can distort the common shape. No
hidden speed multiplier or automatic range change is applied.

Only living, in-world, same-map/instance members count towards the travel layout.
Joins/leaves, resurrection and role changes legitimately recompute slots; an
unchanged roster has no timer/random reordering. Larger groups wrap rows at the
native MAX_GROUP_SIZE (five) rather than forming a line of unrestricted width.
Empty role rows consume no space. Missing/mismatched groups, non-player/unsafe
targets and invalid spacing/heights produce the normal null location. Combat
and dead/ghost following use the existing circle policy instead of travel rows.

Formation selection, save/load/reset and help use the existing native value/chat
paths. Normal follow angle/distance comparisons and movement generators retain
pathfinding, collision and recovery ownership. There is no new queue, timer,
teleport, speed override or map repair. The native combat/loot/gather actions can
temporarily leave formation and normal following can resume afterwards.

Geometry does not guarantee an exact rectangle through a doorway or on broken
collision data. Native pathfinding can alter a route or reject an unreachable
slot. It cannot guarantee NPC interaction when the master stands at the edge
of the NPC's range. These remain important live checks, alongside turns, ramps,
crowd clearance and return after combat/harvesting. Alon's 8% movement talent and
the separate speed/jitter investigation are unchanged.

Tests should execute the production formation and native loader/command methods,
not a copied layout algorithm. Validate role rows, facing/translation, repeated
getters, configured ranges, GUID-stable ordering, membership/role changes, group
sizes, ground/flight and invalid contexts, combat fallback, save/load/help/reset,
supported variants and existing follow/circle/master-focus consumers. Source
checks and native-header compilation are not live route or client acceptance.

# Native movement state before gathering casts

Issue #86, 25 October 2026. Source-only correction on 020e65d4; full image,
deployment and live acceptance remain pending. No live range/strategy changes.

## Captured behavior

Flore repeatedly approached Peacebloom 1618, spawn 31027, GUID
17370383789913569587 near (-10275.3,1933.49,33.0991). The master stayed about
12.6 yards from the node, within the configured 15-yard active-master gathering
range. Saved Herbalism was 102/150. Opens at 19:41:14.406/35.442/56.402 and
later cycles were followed by ordinary Follow and stalled/ten-second retries,
without a successful loot response in the captured post-build interval.
See the raw files under /home/paul/development/flore-harvest-20261025.

At 19:41:35.442 opening reports OK; at 35.543 native movement flags are
0x400001, unit state is zero and the spline is active. Follow resumes at
19:41:40.455. The approximately 21-second repeat cycle is an observation, not
a new timer requirement. SpellStart confirms acceptance, not completed loot.

## Confirmed contract and correction

WorldObject::IsMoving reads the movement-flag mask and says true. Legacy
Unit::IsStopped reads generator-state UNIT_STAT_MOVING and also says true
because that separate state bit is unset. The ZERO compatibility facade used
!IsStopped and incorrectly said false. GameObject CastSpell uses that facade
to decide whether to stop before a cast-time/channel spell, so this state
skips its existing StopMoving call. Native Spell::update checks displacement
over 0.5 yards for movement-interruptible casts. Herb Gathering 2366 and Mining
2575 both have interruptFlags 31 in the inspected server spell definitions.

The ZERO predicate now reuses native WorldObject::IsMoving plus its existing
explicit falling check. All callers share that one compatibility correction:
unit/game-object/ground cast preparation, item use, facing, mount/travel/stay
and formation/snare checks. Non-ZERO predicates are unchanged. Native stop,
spell/channel/instant/jump rules, safety, LOS, skill, loot and follow geometry
remain in their responsible implementations. No bot/item/map identity, extra
leash radius, artificial wait or gameplay exception is introduced.

The saved 2.5/3-yard follow settings remain appropriate follow settings; they
are not replaced with a larger harvesting radius. This correction addresses
a reproduced moving/stopped mismatch. The exact live spell-cancel packet was
not captured, and additional loot/follow ownership issues are not ruled out.
Do not label all gathering-range symptoms resolved before a live test.

## Executable coverage and limits

tests/playerbot-native-movement extracts the real native movement enums,
WorldObject moving and Unit stopped methods, facade function, game-object cast
stop block and native movement-interruption condition. The captured state
reproduces old facade=false / StopMoving calls=0 / movement interruption=true;
the correction gives true / 1 / false after stopping. Instant, channeled,
jump/fall and unattended-bot rules remain exercised. Four predicate macro
branches compile and pass; this is not a full TBC/Wrath server build.

These fixtures model services and motion advancement; they do not record a
complete real Spell lifecycle or prove the actual live cancellation reason.
Keep #86/#58 open for image compilation and actual harvest/loot/follow testing.

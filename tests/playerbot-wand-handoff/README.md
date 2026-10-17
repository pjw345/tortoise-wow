# Wand handoff regression

Configure/build this folder with CMake, then run CTest. The test compiles the
production cast-prerequisite and interrupt methods, interrupt class declaration,
wand update, global cooldown manager methods, and strict GCD rejection block.
The remaining game services are stubs; this is not a full server integration test.

It demonstrates repeated wand swings rejecting a normal spell, then a successful
wand-only prerequisite leaving the cooldown intact and allowing the spell after
the wait. It also covers known/unknown/unresolved spells, policy rejection,
spell cooldown, self/party targets, missing target, repeat interruption, an
active generic heal, and hunter auto shot. The extraction checks native Engine
prerequisite ordering and retention of the requested action.

Pending gameplay acceptance: a priest must leave wand fire for an eligible heal;
above 50% mana it must resume eligible DPS spells; at/below 50% it must keep the
damage-spell reserve while healing remains available. Check master-target changes
and stationary ranged finishing of runners separately. This change does not
configure Alon's role or prove healing coordination, geometry or target policy.
Normal native failures (mana, LOS, range, cooldown, aura requirements) still apply
after the prerequisite and can reject the requested spell.

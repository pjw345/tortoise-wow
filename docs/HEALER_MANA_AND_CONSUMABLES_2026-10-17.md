# Healer reserve and consumable investigation, 2026-10-17

## Release scope

Based on merged playerbots-development 5277746008f73ff1c91a9dcdb58405770adea390.
This candidate implements the healer damage cutoff only. At or below 50% mana,
human-owned healers cannot start direct damage or damaging aura spells; above 50%
they may use their existing DPS rotation. Healing and utility remain allowed.
Wand auto-repeat and ordinary melee remain subject to native target/range/LOS
rules. Existing DoTs are not cancelled. Priest offdps gains a regular, lower
priority wand fallback rather than waiting for its old no-mana threshold.

## Confirmed blockers in the merged source

- UseItemAction.h ThrowGrenadeAction requires Engineering 175 and level 52, chooses
  only 4390/15993/23737, and refuses a stationary noncasting target. It never
  selects owned Rough Dynamite (4358). ClassStrategy.cpp also schedules the action
  only from ranged medium AoE. Owning or crafting Rough Dynamite does not make
  this action choose it.
- UsePotionsStrategy.cpp schedules low mana -> dark rune -> mana potion alternative.
  UseDarkRuneAction::isUseful rejects health <=1000 and mages. ReactionEngine::
  FindReaction only queues alternatives after isPossible fails; an isUseful
  rejection discards the action and its alternative. Thus low-level bots can
  never reach that mana-potion fallback.
- FindPotionVisitor only accepts consumable subclasses potion/flask. Earlier
  read-only world-data inspection found Minor Mana Potion and Weak Troll's Blood
  recorded in generic consumable subclass 0. This is a selection incompatibility;
  no fresh production DB query was run while the services are stopped.
- The merged strategy has no general inventory buff-potion action. Being an
  alchemist/herbalist does not itself schedule a Troll's Blood buff.
- Potions are installed on the reaction engine by AiFactory. Absence from co ?
  is not evidence that the potion strategy is disabled.

## Why this does not prove the failed PR #34 is fixed

PR #34 (56abf64a) already adds owned-item selection, a direct low-mana potion
reaction and buff-potion use. It was excluded from the accepted merges because
gameplay still showed no use. The baseline blockers above explain the merged
source but cannot alone explain that earlier combined candidate.

Additional constraints in #34: the grenade still needs the ranged-medium-AoE
trigger and its friendly/CC/unengaged-neighbour blast-safety checks; the direct
inventory mana potion refuses while casting; buff potions require the bot to be
stationary, standing, out of combat and mostly recovered. These can suppress
use, but which gate actually rejected Alon/Flore has not been observed at runtime.
Do not remove safety gates or claim consumables repaired from this review.

Next investigation should use the existing action diagnostics to distinguish
trigger not queued, isUseful/isPossible rejection, item selection/cooldown and
native spell execution result. Capture bot role/strategies, current item/skill,
target/mob count, nearby CC/unengaged mobs and casting state. Verify real item
count decreases after accepted use. Keep free-bot and item-cheat semantics separate.

## Primary upstream search

- https://github.com/cmangos/playerbots/blob/master/playerbot/strategy/actions/UseItemAction.h
  has the same family of item-use contracts; compatibility must still be checked.
- https://github.com/ike3/mangosbot-bots/blob/master/playerbot/strategy/generic/UsePotionsStrategy.cpp
  schedules low mana directly to mana potion in its older engine. This supports
  the direct scheduling approach, not wholesale porting between different engines.

## Acceptance

Fight with an equipped wand and a healing/offdps strategy. Cross 50% in both
directions: damage spells should stop at 50% or below, healing should continue,
wand should fire when healing is not needed and native targeting permits it.
Above 50%, damage spells may resume. Repeat after leader death. Confirm a
non-healer's rotation is unchanged. Consumables remain a separate open test.

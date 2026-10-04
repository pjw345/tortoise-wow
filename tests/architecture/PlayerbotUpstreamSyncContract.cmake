# Focused source-contract regression for the selectively adapted upstream
# Playerbot fixes. This does not replace compilation or live gameplay tests.
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/WaitForAttackAction.cpp" waitForAttack)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MovementActions.cpp" movement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MoveToRpgTargetAction.cpp" rpgMovement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MoveToTravelTargetAction.cpp" travelMovement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/generic/CombatStrategy.h" combatStrategy)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/generic/DeadStrategy.cpp" deadStrategy)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/paladin/PaladinActions.cpp" paladinActions)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/paladin/PaladinActions.h" paladinActionDefinitions)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/LootObjectStack.cpp" lootStack)

string(FIND "${combatStrategy}" "sPlayerbotAIConfig.waitForAttackDistance" configuredDistance)
if(configuredDistance EQUAL -1)
    message(FATAL_ERROR "Wait-for-attack must retain the dedicated configured distance")
endif()
foreach(required
    "const float safeDistance = WaitForAttackStrategy::GetSafeDistance()"
    "WorldPosition(bot).fDist(WorldPosition(target)) > safeDistance"
    "std::min_element(points.begin(), points.end()"
    "enemy && enemy->CanAttackOnSight(bot)"
    "WaitForAttackStrategy::GetSafeDistance() <= ai->GetRange(\"guard\")")
    string(FIND "${waitForAttack}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Wait-for-attack safe-position adaptation missing: ${required}")
    endif()
endforeach()
string(FIND "${waitForAttack}" "target->GetAttackDistance(bot) + ATTACK_DISTANCE), WaitForAttackStrategy::GetSafeDistance()" inheritedAttackRange)
if(NOT inheritedAttackRange EQUAL -1)
    message(FATAL_ERROR "Wait-for-attack must not expand the configured compact distance")
endif()

string(FIND "${movement}" "WaitForReach(std::max(0.0f, dist - approachDistance))" lootWait)
if(lootWait EQUAL -1)
    message(FATAL_ERROR "Loot approach must wait for its launched movement")
endif()

foreach(movementSource rpgMovement travelMovement)
    string(FIND "${${movementSource}}" "LootObject lootTarget = AI_VALUE(LootObject, \"loot target\")" selectedLoot)
    string(FIND "${${movementSource}}" "if (lootTarget.IsLootPossible(bot))" selectedLootGuard)
    if(selectedLoot EQUAL -1 OR selectedLootGuard LESS selectedLoot)
        message(FATAL_ERROR "RPG/travel movement must yield to the selected loot target: ${movementSource}")
    endif()
endforeach()

foreach(required
    "go->GetGOInfo()->type == GAMEOBJECT_TYPE_CHEST"
    "go->GetGOInfo()->GetLootId()"
    "!go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED)")
    string(FIND "${lootStack}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Dynamically unlocked chest handling missing: ${required}")
    endif()
endforeach()

string(FIND "${deadStrategy}" "new NextAction(\"accept resurrect\", relevance + 10.0f)" resurrectPriority)
if(resurrectPriority EQUAL -1)
    message(FATAL_ERROR "Offered resurrection must outrank automatic corpse recovery")
endif()

# A party target can acquire this paladin's blessing after target selection but
# before the action executes. Revalidate caster ownership in both self and party
# blessing paths so the action does not replace its own active blessing.
string(FIND "${paladinActions}" "bool HasOwnedBlessing(PlayerbotAI* ai, Unit* target)" ownershipHelperStart)
string(FIND "${paladinActions}" "bool CastPaladinAuraAction::Execute" ownershipHelperEnd)
if(ownershipHelperStart EQUAL -1 OR ownershipHelperEnd LESS ownershipHelperStart)
    message(FATAL_ERROR "Cannot isolate the paladin blessing ownership helper")
endif()
math(EXPR ownershipHelperLength "${ownershipHelperEnd} - ${ownershipHelperStart}")
string(SUBSTRING "${paladinActions}" ${ownershipHelperStart} ${ownershipHelperLength} ownershipHelper)

foreach(required
    "bool HasOwnedBlessing(PlayerbotAI* ai, Unit* target)"
    "ai->HasMyAura(blessing, target)"
    "ai->HasMyAura(\"greater \" + blessing, target)"
    "\"blessing of might\""
    "\"blessing of wisdom\""
    "\"blessing of kings\""
    "\"blessing of sanctuary\""
    "\"blessing of salvation\""
    "\"blessing of light\"")
    string(FIND "${ownershipHelper}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing paladin blessing ownership contract: ${required}")
    endif()
endforeach()

string(REGEX MATCHALL "HasOwnedBlessing\\(ai, target\\)" ownershipChecks "${paladinActions}")
list(LENGTH ownershipChecks ownershipCheckCount)
if(NOT ownershipCheckCount EQUAL 2)
    message(FATAL_ERROR "Paladin blessing ownership must be revalidated in self and party actions")
endif()

# The generic spell guard evaluates isUseful() before isPossible(). Dynamic
# blessing actions do not resolve their real spell ID until isPossible(), so
# both the self and party variants must deliberately defer capability checks to
# that stage. Fixed-spell manual blessing actions retain the normal guard.
function(assert_dynamic_blessing className nextClassName)
    string(FIND "${paladinActionDefinitions}" "class ${className}" classStart)
    string(FIND "${paladinActionDefinitions}" "class ${nextClassName}" classEnd)
    if(classStart EQUAL -1 OR classEnd LESS classStart)
        message(FATAL_ERROR "Cannot isolate dynamic blessing class: ${className}")
    endif()
    math(EXPR classLength "${classEnd} - ${classStart}")
    string(SUBSTRING "${paladinActionDefinitions}" ${classStart} ${classLength} classDefinition)
    string(FIND "${classDefinition}" "bool isUseful() override { return true; }" usefulOverride)
    if(usefulOverride EQUAL -1)
        message(FATAL_ERROR "Dynamic blessing action must reach isPossible(): ${className}")
    endif()
endfunction()

assert_dynamic_blessing(CastBlessingAction CastPveBlessingAction)
assert_dynamic_blessing(CastBlessingOnPartyAction CastPveBlessingOnPartyAction)

message(STATUS "PASS: selective Playerbot upstream movement, loot, chest, resurrection and blessing contracts")

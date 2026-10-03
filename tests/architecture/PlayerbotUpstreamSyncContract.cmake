# Focused source-contract regression for the selectively adapted upstream
# Playerbot fixes. This does not replace compilation or live gameplay tests.
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/WaitForAttackAction.cpp" waitForAttack)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MovementActions.cpp" movement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MoveToRpgTargetAction.cpp" rpgMovement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/MoveToTravelTargetAction.cpp" travelMovement)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/generic/CombatStrategy.h" combatStrategy)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/generic/DeadStrategy.cpp" deadStrategy)
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

message(STATUS "PASS: selective Playerbot upstream movement, loot, chest and resurrection contracts")

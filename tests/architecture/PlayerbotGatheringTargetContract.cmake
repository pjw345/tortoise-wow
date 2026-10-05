# Ordinary mining and herbalism nodes are game objects. Their gathering spells
# must receive the node explicitly instead of relying on the Unit overload to
# rediscover the current loot target from a spell-effect-slot heuristic.
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/LootAction.cpp" lootAction)

string(FIND "${lootAction}" "bool OpenLootAction::DoLoot" doLootStart)
string(FIND "${lootAction}" "uint32 OpenLootAction::GetOpeningSpell" doLootEnd)
if(doLootStart EQUAL -1 OR doLootEnd LESS doLootStart)
    message(FATAL_ERROR "Cannot isolate Playerbot open-loot action")
endif()
math(EXPR doLootLength "${doLootEnd} - ${doLootStart}")
string(SUBSTRING "${lootAction}" ${doLootStart} ${doLootLength} doLoot)

foreach(required
    "return go && ai->HasSkill(SKILL_MINING) ? ai->CastSpell(MINING, go) : false;"
    "return go && ai->HasSkill(SKILL_HERBALISM) ? ai->CastSpell(HERB_GATHERING, go) : false;")
    string(FIND "${doLoot}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gathering spell must explicitly target its game object: ${required}")
    endif()
endforeach()

foreach(forbidden
    "ai->CastSpell(MINING, bot)"
    "ai->CastSpell(HERB_GATHERING, bot)")
    string(FIND "${doLoot}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gathering spell still uses the Unit/self target path: ${forbidden}")
    endif()
endforeach()

message(STATUS "PASS: Playerbot gathering spells explicitly target mining and herbalism nodes")

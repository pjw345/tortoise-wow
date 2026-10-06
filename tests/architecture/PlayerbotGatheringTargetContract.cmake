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

foreach(required
    "bool const pendingGatheringResponse = lootObject.guid.IsGameObject()"
    "lootObject.skillId == SKILL_MINING || lootObject.skillId == SKILL_HERBALISM"
    "if (result && !pendingGatheringResponse)")
    string(FIND "${lootAction}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gathering node must remain pending until its loot response: ${required}")
    endif()
endforeach()

# In this core setGOTarget stores only the object pointer/GUID. Directly
# constructed Playerbot spells must also add the target-mask bit or the cast
# animation can complete without EffectOpenLock receiving the node.
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/PlayerbotAI.cpp" playerbotAI)

foreach(signature
    "bool PlayerbotAI::CanCastSpell(uint32 spellid, GameObject* goTarget"
    "bool PlayerbotAI::CastSpell(uint32 spellId, GameObject* goTarget")
    string(FIND "${playerbotAI}" "${signature}" functionStart)
    if(functionStart EQUAL -1)
        message(FATAL_ERROR "Cannot find Playerbot game-object spell function: ${signature}")
    endif()
    string(SUBSTRING "${playerbotAI}" ${functionStart} 8000 functionBody)
    string(FIND "${functionBody}" "setGOTarget(goTarget);" targetSet)
    string(FIND "${functionBody}" "m_targetMask |= TARGET_FLAG_GAMEOBJECT;" targetMaskSet)
    if(targetSet EQUAL -1 OR targetMaskSet EQUAL -1 OR targetMaskSet LESS targetSet)
        message(FATAL_ERROR "Playerbot game-object spell must set TARGET_FLAG_GAMEOBJECT after setGOTarget: ${signature}")
    endif()
endforeach()

message(STATUS "PASS: Playerbot gathering spells target, mask and retain mining/herbalism nodes until loot response")

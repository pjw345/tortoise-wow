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
    "ai->CastSpell(MINING, go, nullptr, true, &spellDuration);"
    "ai->CastSpell(HERB_GATHERING, go, nullptr, true, &spellDuration);"
    "ai->CastSpell(spellId, go, nullptr, true, &spellDuration);"
    "ai->CastSpell(ENGINEERING, creature, nullptr, true, &spellDuration);"
    "ai->CastSpell(32605, creature, nullptr, true, &spellDuration);"
    "ai->CastSpell(32606, creature, nullptr, true, &spellDuration);"
    "ai->CastSpell(SKINNING, creature, nullptr, true, &spellDuration);"
    "SetDuration(spellDuration);")
    string(FIND "${doLoot}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gathering/opening casts must preserve their explicit target and duration: ${required}")
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
    "bool const gatheringGameObject = lootObject.guid.IsGameObject()"
    "lootObject.skillId == SKILL_MINING || lootObject.skillId == SKILL_HERBALISM"
    "lootStack->Defer(lootObject.guid, sPlayerbotAIConfig.lootTargetRetryDelay)"
    "context->GetValue<LootObject>(\"loot target\")->Set(LootObject())"
    "bool opened = ai->CastSpell(spellId, go, nullptr, true, &spellDuration);")
    string(FIND "${lootAction}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Game-object loot retry/target contract is missing: ${required}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/LootObjectStack.h" lootStackHeader)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/LootObjectStack.cpp" lootStackSource)

foreach(required
    "void Defer(ObjectGuid guid, uint32 seconds);"
    "std::map<ObjectGuid, time_t> deferredLoot;")
    string(FIND "${lootStackHeader}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gathering retry queue contract is missing: ${required}")
    endif()
endforeach()

foreach(required
    "void LootObjectStack::Defer(ObjectGuid guid, uint32 seconds)"
    "deferredLoot[guid] = time(0) + std::max<uint32>(1, seconds);"
    "if (deferredLoot.find(guid) != deferredLoot.end())")
    string(FIND "${lootStackSource}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gathering retry implementation is missing: ${required}")
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

message(STATUS "PASS: Playerbot game-object loot uses explicit targets and bounded gathering retries")

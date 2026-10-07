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
    "bool const gatheringGameObject = lootObject.guid.IsGameObject()"
    "lootObject.skillId == SKILL_MINING || lootObject.skillId == SKILL_HERBALISM"
    "lootStack->Defer(lootObject.guid, sPlayerbotAIConfig.lootTargetRetryDelay)"
    "context->GetValue<LootObject>(\"loot target\")->Set(LootObject())"
    "bool opened = ai->CastSpell(spellId, go);")
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

# A cast accepted asynchronously can still be rejected by the core's strict
# completion check. Preserve the native result so gathering failures can be
# distinguished without changing spell behaviour.
file(READ "${SOURCE_ROOT}/src/game/Spells/Spell.cpp" spellSource)
foreach(required
    "BotActionLog_LogCastFailure(m_caster, m_spellInfo->Id, uint8(castResult), \"completion-power\");"
    "BotActionLog_LogCastFailure(m_caster, m_spellInfo->Id, uint8(castResult), \"completion-check\");")
    string(FIND "${spellSource}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Native cast completion result diagnostic is missing: ${required}")
    endif()
endforeach()

message(STATUS "PASS: Playerbot game-object loot uses explicit targets and bounded gathering retries")

# SkillLineEntry::name has MAX_DBC_LOCALE entries, while MAX_LOCALE also
# includes the non-vanilla ruRU slot. A name-filtered Playerbot skill command
# must never scan or format the DBC array with the wider locale bound.
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/strategy/actions/SkillAction.cpp" skillAction)
file(READ "${SOURCE_ROOT}/modules/mod-playerbots/src/playerbot/ChatHelper.cpp" chatHelper)

string(FIND "${chatHelper}" "std::string ChatHelper::formatSkill" formatSkillStart)
string(FIND "${chatHelper}" "std::string ChatHelper::formatReaction" formatSkillEnd)
if(formatSkillStart EQUAL -1 OR formatSkillEnd LESS formatSkillStart)
    message(FATAL_ERROR "Cannot isolate Playerbot skill formatter")
endif()
math(EXPR formatSkillLength "${formatSkillEnd} - ${formatSkillStart}")
string(SUBSTRING "${chatHelper}" ${formatSkillStart} ${formatSkillLength} formatSkill)

foreach(required
    "if (!requester)"
    "if (requester->GetSession())"
    "sessionLocale < MAX_DBC_LOCALE"
    "locale < MAX_DBC_LOCALE"
    "localizedName && localizedName[0]")
    string(FIND "${skillAction}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Playerbot skill lookup locale guard missing: ${required}")
    endif()
endforeach()

string(FIND "${skillAction}" "MAX_LOCALE" wideLocaleBound)
if(NOT wideLocaleBound EQUAL -1)
    message(FATAL_ERROR "Playerbot skill lookup must use the eight-entry DBC locale bound")
endif()

foreach(required
    "loc_idx < 0 || loc_idx >= int(MAX_DBC_LOCALE)"
    "locale < MAX_DBC_LOCALE"
    "localizedName && localizedName[0]")
    string(FIND "${formatSkill}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Playerbot skill formatting locale guard missing: ${required}")
    endif()
endforeach()

message(STATUS "PASS: Playerbot skill lookup and formatting stay within DBC locale bounds")

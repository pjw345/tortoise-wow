cmake_minimum_required(VERSION 3.16)
project(PartyHealingTest LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    extract.py
    ../../modules/mod-playerbots/src/playerbot/strategy/actions/ChooseTargetActions.cpp
    ../../modules/mod-playerbots/src/playerbot/PlayerbotAI.cpp
    ../../modules/mod-playerbots/src/playerbot/strategy/values/PartyMemberToHeal.cpp
    ../../modules/mod-playerbots/src/playerbot/strategy/triggers/HealthTriggers.cpp
    ../../modules/mod-playerbots/src/playerbot/strategy/actions/GenericSpellActions.cpp)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
    "${CMAKE_CURRENT_BINARY_DIR}/NativeHealing.inc" RESULT_VARIABLE extracted)
if(NOT extracted EQUAL 0)
    message(FATAL_ERROR "Native party healing extraction failed")
endif()
add_executable(PartyHealingTest PartyHealingTest.cpp)
target_compile_features(PartyHealingTest PRIVATE cxx_std_17)
target_include_directories(PartyHealingTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME party_healing COMMAND PartyHealingTest)

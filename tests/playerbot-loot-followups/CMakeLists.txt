cmake_minimum_required(VERSION 3.16)
project(PlayerbotLootFollowups LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract.py
 ../../modules/mod-playerbots/src/playerbot/strategy/actions/LootAction.cpp
 ../../modules/mod-playerbots/src/playerbot/strategy/values/LootValues.cpp)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
 "${CMAKE_CURRENT_BINARY_DIR}/NativeLoot.inc" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
 message(FATAL_ERROR "Native loot extraction failed")
endif()
add_executable(LootFollowupsTest LootFollowupsTest.cpp)
target_compile_features(LootFollowupsTest PRIVATE cxx_std_17)
target_include_directories(LootFollowupsTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME native_loot_followups COMMAND LootFollowupsTest)

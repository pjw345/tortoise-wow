cmake_minimum_required(VERSION 3.16)
project(GatheringSafetyTest LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS extract.py
 ../../modules/mod-playerbots/src/playerbot/strategy/actions/AddLootAction.cpp
 ../../modules/mod-playerbots/src/playerbot/LootObjectStack.cpp)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
 "${CMAKE_CURRENT_BINARY_DIR}/NativeGathering.inc" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
 message(FATAL_ERROR "Native gathering extraction failed")
endif()
add_executable(GatheringSafetyTest GatheringSafetyTest.cpp)
target_compile_features(GatheringSafetyTest PRIVATE cxx_std_17)
target_include_directories(GatheringSafetyTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME native_gathering_safety COMMAND GatheringSafetyTest)

cmake_minimum_required(VERSION 3.16)
project(PlayerbotFollowTests LANGUAGES CXX)
enable_testing()
set(CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(BOT_ROOT "${CORE_ROOT}/modules/mod-playerbots/src/playerbot")
function(extract source start end output)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" body)
    string(FIND "${body}" "${start}" first)
    if(first LESS 0)
        message(FATAL_ERROR "Missing native entry: ${output}")
    endif()
    string(SUBSTRING "${body}" ${first} -1 body)
    string(FIND "${body}" "${end}" length)
    if(length LESS 0)
        message(FATAL_ERROR "Missing native boundary: ${output}")
    endif()
    string(SUBSTRING "${body}" 0 ${length} fragment)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${output}" "${fragment}")
endfunction()
extract("${CORE_ROOT}/src/game/Movement/TargetedMovementGenerator.h"
    "class TargetedMovementGeneratorBase" "template<class T>\nclass ChaseMovementGenerator" NativeTargetedState.inc)
extract("${BOT_ROOT}/ServerFacade.cpp"
    "// Inspect only the active native generator" "bool ServerFacade::isMoving" NativeMovementInspection.inc)
extract("${BOT_ROOT}/strategy/actions/MovementActions.cpp"
    "    if (sServerFacade.IsFollowing(bot, target, formationDistance, angle))"
    "WorldPosition CalculatePerpendicularPoint" NativeFollowDispatch.inc)
add_executable(FollowMovementInspectionTest FollowMovementInspectionTest.cpp)
target_compile_features(FollowMovementInspectionTest PRIVATE cxx_std_17)
target_include_directories(FollowMovementInspectionTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(FollowMovementInspectionTest PRIVATE /EHsc)
endif()
add_test(NAME FollowMovementInspectionTest COMMAND FollowMovementInspectionTest)
set_tests_properties(FollowMovementInspectionTest PROPERTIES TIMEOUT 20)
# All three callers must use the same normalized comparison, rather than
# comparing the raw formation distance to the native offset independently.
foreach(source strategy/actions/FollowActions.cpp strategy/triggers/RangeTriggers.h)
    file(READ "${BOT_ROOT}/${source}" caller)
    string(FIND "${caller}" "sServerFacade.IsFollowing(bot, followTarget, formation->GetOffset(), formation->GetAngle())" admission)
    if(admission LESS 0)
        message(FATAL_ERROR "Follow admission disagrees with dispatch: ${source}")
    endif()
endforeach()

# Existing movement consumers, without unrelated stale architecture extractions.
extract("${BOT_ROOT}/strategy/actions/MovementActions.cpp"
    "void MovementAction::DispatchMovement(" "Unit* MovementAction::GetMover(" BotMoveDispatch.inc)
extract("${CORE_ROOT}/src/game/Movement/MotionMaster.cpp"
    "void MotionMaster::MovePath(" "void MotionMaster::MoveWaypointAsDefault(" BotNativeMovePath.inc)
extract("${CORE_ROOT}/src/game/Movement/PointMovementGenerator.cpp"
    "template<class T>\nvoid PointMovementGenerator<T>::Initialize(" "template<class T>\nvoid PointMovementGenerator<T>::Finalize(" BotNativePointInit.inc)
extract("${CORE_ROOT}/src/game/Movement/PointMovementGenerator.cpp"
    "template<class T>\nbool PointMovementGenerator<T>::Update(" "template<>\nvoid PointMovementGenerator<Player>::MovementInform(" BotNativePointUpdate.inc)
add_executable(BotMovementDispatchTest ../architecture/BotMovementDispatchTest.cpp)
target_compile_features(BotMovementDispatchTest PRIVATE cxx_std_17)
target_compile_definitions(BotMovementDispatchTest PRIVATE MANGOSBOT_ZERO)
target_include_directories(BotMovementDispatchTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(BotMovementDispatchTest PRIVATE /EHsc)
endif()
add_test(NAME BotMovementDispatchTest COMMAND BotMovementDispatchTest)
set_tests_properties(BotMovementDispatchTest PROPERTIES TIMEOUT 20)

extract("${BOT_ROOT}/WorldPosition.cpp"
    "bool WorldPosition::ClosestCorrectPoint(" "bool WorldPosition::GetReachableRandomPointOnGround(" BotRpgPoint.inc)
extract("${BOT_ROOT}/strategy/actions/ChooseRpgTargetAction.cpp"
    "std::unordered_map<ObjectGuid, uint32> ChooseRpgTargetAction::GetTargetCounts(" "#define SkipRpgTarget" BotRpgCrowd.inc)
extract("${BOT_ROOT}/strategy/actions/MoveToRpgTargetAction.cpp"
    "    WorldPosition movePos(mapId, x, y, z);" "    if ((ai->HasStrategy(\"debug rpg\"" BotRpgApproach.inc)
extract("${BOT_ROOT}/strategy/actions/MoveToRpgTargetAction.cpp"
    "bool MoveToRpgTargetAction::isUseful()" "    if(MEM_AI_VALUE" BotRpgUsefulPrefix.inc)
add_executable(BotRpgMovementTest ../architecture/BotRpgMovementTest.cpp)
target_compile_features(BotRpgMovementTest PRIVATE cxx_std_17)
target_include_directories(BotRpgMovementTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(BotRpgMovementTest PRIVATE /EHsc)
endif()
add_test(NAME BotRpgMovementTest COMMAND BotRpgMovementTest)
set_tests_properties(BotRpgMovementTest PROPERTIES TIMEOUT 20)

cmake_minimum_required(VERSION 3.21)
project(uav_perception VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UavCommon.cmake)
uav_use_common()
find_package(Threads REQUIRED)

add_library(perception_lib STATIC
    src/obstacle_detector.cpp)
target_include_directories(perception_lib PUBLIC include)
target_link_libraries(perception_lib PUBLIC uav::common)
uav_set_warnings(perception_lib)

add_executable(perception src/main.cpp)
target_link_libraries(perception PRIVATE perception_lib Threads::Threads)
uav_set_warnings(perception)

install(TARGETS perception RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(perception_tests tests/test_obstacle_detector.cpp)
    target_link_libraries(perception_tests PRIVATE perception_lib)
    add_test(NAME perception.obstacle_detector COMMAND perception_tests)
endif()

cmake_minimum_required(VERSION 3.21)
project(uav_simulator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UavCommon.cmake)
uav_use_common()

add_library(sim_core STATIC
    src/simulation.cpp)
target_include_directories(sim_core PUBLIC include)
target_link_libraries(sim_core PUBLIC uav::common)
uav_set_warnings(sim_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(sim_tests tests/test_simulation.cpp)
    target_link_libraries(sim_tests PRIVATE sim_core)
    add_test(NAME simulator.lockstep COMMAND sim_tests)
endif()

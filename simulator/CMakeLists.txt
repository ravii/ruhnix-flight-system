cmake_minimum_required(VERSION 3.21)
project(rfs_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/RfsCommon.cmake)
rfs_use_common()

# ---- sim_core: lockstep loop, vehicle and sensor models -------------------
add_library(sim_core STATIC
    src/imu_sensor_model.cpp
    src/quadrotor_model.cpp
    src/simulation.cpp)
target_include_directories(sim_core PUBLIC include)
target_link_libraries(sim_core PUBLIC rfs::common)
rfs_set_warnings(sim_core)

# ---- sim_fc: algorithm loop, flight-controller core in-process ------------
rfs_use_fc_core()
add_library(sim_fc STATIC
    src/in_process_link.cpp)
target_link_libraries(sim_fc PUBLIC sim_core rfs::fc_core)
rfs_set_warnings(sim_fc)

# ---- sim_udp: SITL link to the firmware (bsp/sitl) over UDP, POSIX only ---
add_library(sim_udp STATIC
    src/udp_link.cpp)
target_link_libraries(sim_udp PUBLIC sim_core)
rfs_set_warnings(sim_udp)

add_executable(rfs_sim src/main.cpp)
target_link_libraries(rfs_sim PRIVATE sim_fc sim_udp)
rfs_set_warnings(rfs_sim)

include(CTest)
if(BUILD_TESTING)
    add_executable(sim_tests tests/test_simulation.cpp)
    target_link_libraries(sim_tests PRIVATE sim_core)
    add_test(NAME simulator.lockstep COMMAND sim_tests)

    add_executable(sim_test_quadrotor tests/test_quadrotor_model.cpp)
    target_link_libraries(sim_test_quadrotor PRIVATE sim_core)
    rfs_set_warnings(sim_test_quadrotor)
    add_test(NAME simulator.quadrotor_model COMMAND sim_test_quadrotor)

    add_executable(sim_test_closed_loop tests/test_closed_loop.cpp)
    target_link_libraries(sim_test_closed_loop PRIVATE sim_fc)
    rfs_set_warnings(sim_test_closed_loop)
    add_test(NAME simulator.closed_loop COMMAND sim_test_closed_loop)

    add_test(NAME simulator.rfs_sim_runs COMMAND rfs_sim --duration 1)
endif()

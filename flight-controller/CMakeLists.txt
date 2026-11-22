cmake_minimum_required(VERSION 3.21)
project(rfs_flight_controller VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/RfsCommon.cmake)

# Board to build firmware for (a directory under bsp/). Empty = host build:
# core + simulator board + unit tests.
set(FC_BOARD "" CACHE STRING "Target board under bsp/ (empty for host build)")

if(FC_BOARD)
    # The board file sets CPU/ABI flags; include it before creating any target
    # so that common/, core/ and FreeRTOS are all compiled with them.
    set(_board_file ${CMAKE_CURRENT_SOURCE_DIR}/bsp/${FC_BOARD}/board.cmake)
    if(NOT EXISTS ${_board_file})
        message(FATAL_ERROR
            "Board '${FC_BOARD}' not found (${_board_file}). "
            "Create it from bsp/_template/ - see bsp/README.md.")
    endif()
    enable_language(ASM)
    include(${_board_file})
endif()

rfs_use_common()

# ---- core: platform-independent algorithms (host-testable) ----------------
add_library(fc_core STATIC
    core/src/esc/dshot.cpp
    core/src/esc/esc_output.cpp
    core/src/flight_control.cpp
    core/src/mixer.cpp
    core/src/pid.cpp)
target_include_directories(fc_core PUBLIC
    core/include
    bsp/include)    # rfs::fc::Board interface; header-only, no hardware dependency
target_link_libraries(fc_core PUBLIC rfs::common)
rfs_set_warnings(fc_core)

# ---- simulator board: rfs::fc::Board backed by a simulator --------------------
add_library(fc_bsp_sim STATIC
    bsp/sim/sim_board.cpp)
target_include_directories(fc_bsp_sim PUBLIC bsp/sim)
target_link_libraries(fc_bsp_sim PUBLIC fc_core)
rfs_set_warnings(fc_bsp_sim)

if(FC_BOARD)
    # ---- firmware: FreeRTOS + application ------------------------------------
    include(${RFS_ROOT}/cmake/FreeRTOS.cmake)
    rfs_add_freertos(
        PORT       ${FC_FREERTOS_PORT}
        HEAP       4
        CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

    add_executable(flight_controller
        app/control_task.cpp
        app/main.cpp
        app/runtime.cpp
        app/telemetry_task.cpp)
    target_include_directories(flight_controller PRIVATE app)
    target_link_libraries(flight_controller PRIVATE fc_core fc_bsp freertos_kernel)
    rfs_set_warnings(flight_controller)

    if(CMAKE_CROSSCOMPILING)
        set_target_properties(flight_controller PROPERTIES SUFFIX ".elf")
        target_link_options(flight_controller PRIVATE
            -Wl,-Map=$<TARGET_FILE_DIR:flight_controller>/flight_controller.map)
        add_custom_command(TARGET flight_controller POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:flight_controller>
                    $<TARGET_FILE_DIR:flight_controller>/flight_controller.bin
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:flight_controller>
                    $<TARGET_FILE_DIR:flight_controller>/flight_controller.hex
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:flight_controller>
            VERBATIM)
    endif()
else()
    # ---- host: unit tests ---------------------------------------------------
    include(CTest)
    if(BUILD_TESTING)
        add_executable(fc_test_core tests/test_core.cpp)
        target_link_libraries(fc_test_core PRIVATE fc_core)
        add_test(NAME fc.core COMMAND fc_test_core)

        add_executable(fc_test_esc tests/test_esc.cpp)
        target_include_directories(fc_test_esc PRIVATE config)  # esc_config.hpp
        target_link_libraries(fc_test_esc PRIVATE fc_core)
        add_test(NAME fc.esc COMMAND fc_test_esc)

        add_executable(fc_test_sim_board tests/test_sim_board.cpp)
        target_link_libraries(fc_test_sim_board PRIVATE fc_bsp_sim)
        add_test(NAME fc.sim_board COMMAND fc_test_sim_board)
    endif()
endif()

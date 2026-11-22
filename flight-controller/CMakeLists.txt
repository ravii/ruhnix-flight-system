cmake_minimum_required(VERSION 3.21)
project(uav_flight_controller VERSION 0.1.0 LANGUAGES C)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UavCommon.cmake)
uav_use_common()

# ---- core: platform-independent algorithms (host-testable) ----------------
add_library(fc_core STATIC
    core/src/pid.c
    core/src/mixer.c)
target_include_directories(fc_core PUBLIC core/include)
target_compile_features(fc_core PUBLIC c_std_11)
target_link_libraries(fc_core PUBLIC uav::common)
uav_set_warnings(fc_core)

if(CMAKE_CROSSCOMPILING)
    # ---- firmware: FreeRTOS + board support ---------------------------------
    set(FC_BOARD "stm32f4" CACHE STRING "Target board (subdirectory of bsp/)")
    set(_board_file ${CMAKE_CURRENT_SOURCE_DIR}/bsp/${FC_BOARD}/board.cmake)
    if(NOT EXISTS ${_board_file})
        message(FATAL_ERROR
            "Board '${FC_BOARD}' not found (${_board_file}).\n"
            "Create it from bsp/_template/ - see bsp/README.md.")
    endif()

    enable_language(ASM)
    include(${_board_file})

    include(${UAV_ROOT}/cmake/FreeRTOS.cmake)
    uav_add_freertos(
        PORT       ${FC_FREERTOS_PORT}
        HEAP       4
        CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

    add_executable(flight_controller app/main.c)
    set_target_properties(flight_controller PROPERTIES SUFFIX ".elf")
    target_link_libraries(flight_controller PRIVATE fc_core fc_bsp freertos_kernel)
    target_link_options(flight_controller PRIVATE
        -Wl,-Map=$<TARGET_FILE_DIR:flight_controller>/flight_controller.map)
    uav_set_warnings(flight_controller)

    add_custom_command(TARGET flight_controller POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:flight_controller>
                $<TARGET_FILE_DIR:flight_controller>/flight_controller.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:flight_controller>
                $<TARGET_FILE_DIR:flight_controller>/flight_controller.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:flight_controller>
        VERBATIM)
else()
    # ---- host: unit tests ---------------------------------------------------
    include(CTest)
    if(BUILD_TESTING)
        add_executable(fc_test_core tests/test_core.c)
        target_link_libraries(fc_test_core PRIVATE fc_core)
        if(NOT MSVC)
            target_link_libraries(fc_test_core PRIVATE m)
        endif()
        add_test(NAME fc.core COMMAND fc_test_core)
    endif()
endif()

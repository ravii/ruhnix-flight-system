cmake_minimum_required(VERSION 3.21)
project(uav_esc VERSION 0.1.0 LANGUAGES C)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UavCommon.cmake)
uav_use_common()

# ---- core: platform-independent motor control math (host-testable) -------
add_library(esc_core STATIC
    core/src/foc.c)
target_include_directories(esc_core PUBLIC core/include)
target_compile_features(esc_core PUBLIC c_std_11)
target_link_libraries(esc_core PUBLIC uav::common)
if(NOT MSVC)
    target_link_libraries(esc_core PUBLIC m)
endif()
uav_set_warnings(esc_core)

if(CMAKE_CROSSCOMPILING)
    # ---- firmware: FreeRTOS + board support ---------------------------------
    set(ESC_BOARD "stm32g4" CACHE STRING "Target board (subdirectory of bsp/)")
    set(_board_file ${CMAKE_CURRENT_SOURCE_DIR}/bsp/${ESC_BOARD}/board.cmake)
    if(NOT EXISTS ${_board_file})
        message(FATAL_ERROR
            "Board '${ESC_BOARD}' not found (${_board_file}).\n"
            "Create it following flight-controller/bsp/_template/ (target name: esc_bsp, "
            "variable: ESC_FREERTOS_PORT).")
    endif()

    enable_language(ASM)
    include(${_board_file})

    include(${UAV_ROOT}/cmake/FreeRTOS.cmake)
    uav_add_freertos(
        PORT       ${ESC_FREERTOS_PORT}
        HEAP       4
        CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

    add_executable(esc app/main.c)
    set_target_properties(esc PROPERTIES SUFFIX ".elf")
    target_link_libraries(esc PRIVATE esc_core esc_bsp freertos_kernel)
    target_link_options(esc PRIVATE -Wl,-Map=$<TARGET_FILE_DIR:esc>/esc.map)
    uav_set_warnings(esc)

    add_custom_command(TARGET esc POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:esc> $<TARGET_FILE_DIR:esc>/esc.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:esc> $<TARGET_FILE_DIR:esc>/esc.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:esc>
        VERBATIM)
else()
    # ---- host: unit tests ---------------------------------------------------
    include(CTest)
    if(BUILD_TESTING)
        add_executable(esc_test_foc tests/test_foc.c)
        target_link_libraries(esc_test_foc PRIVATE esc_core)
        add_test(NAME esc.foc COMMAND esc_test_foc)
    endif()
endif()

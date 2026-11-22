cmake_minimum_required(VERSION 3.21)
project(uav_esc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UavCommon.cmake)

# Board to build firmware for (a directory under bsp/). Empty = host unit tests.
set(ESC_BOARD "" CACHE STRING "Target board under bsp/ (empty for host build)")

if(ESC_BOARD)
    # Include first so CPU/ABI flags apply to every target below.
    set(_board_file ${CMAKE_CURRENT_SOURCE_DIR}/bsp/${ESC_BOARD}/board.cmake)
    if(NOT EXISTS ${_board_file})
        message(FATAL_ERROR
            "Board '${ESC_BOARD}' not found (${_board_file}). "
            "Create it following flight-controller/bsp/_template/ "
            "(target name: esc_bsp, variable: ESC_FREERTOS_PORT).")
    endif()
    enable_language(ASM)
    include(${_board_file})
endif()

uav_use_common()

# ---- core: platform-independent motor control (host-testable) ------------
add_library(esc_core STATIC
    core/src/current_controller.cpp
    core/src/foc.cpp)
target_include_directories(esc_core PUBLIC
    core/include
    bsp/include)    # esc::Board interface; header-only
target_link_libraries(esc_core PUBLIC uav::common)
uav_set_warnings(esc_core)

if(ESC_BOARD)
    # ---- firmware: FreeRTOS + application ------------------------------------
    include(${UAV_ROOT}/cmake/FreeRTOS.cmake)
    uav_add_freertos(
        PORT       ${ESC_FREERTOS_PORT}
        HEAP       4
        CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

    add_executable(esc
        app/main.cpp
        app/runtime.cpp)
    target_link_libraries(esc PRIVATE esc_core esc_bsp freertos_kernel)
    uav_set_warnings(esc)

    if(CMAKE_CROSSCOMPILING)
        set_target_properties(esc PROPERTIES SUFFIX ".elf")
        target_link_options(esc PRIVATE -Wl,-Map=$<TARGET_FILE_DIR:esc>/esc.map)
        add_custom_command(TARGET esc POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:esc> $<TARGET_FILE_DIR:esc>/esc.bin
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:esc> $<TARGET_FILE_DIR:esc>/esc.hex
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:esc>
            VERBATIM)
    endif()
else()
    # ---- host: unit tests ---------------------------------------------------
    include(CTest)
    if(BUILD_TESTING)
        add_executable(esc_test_core tests/test_core.cpp)
        target_link_libraries(esc_test_core PRIVATE esc_core)
        add_test(NAME esc.core COMMAND esc_test_core)
    endif()
endif()

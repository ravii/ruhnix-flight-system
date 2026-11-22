# Board definition template. Copy this directory to bsp/<your_board>/ and fill it in.
#
# This file is include()d by flight-controller/CMakeLists.txt (same scope), so
# compile/link options set here apply to the whole firmware, including FreeRTOS.
#
# It must:
#   1. set FC_FREERTOS_PORT
#   2. set CPU flags with add_compile_options()/add_link_options()
#   3. define a static library target named `fc_bsp` that implements
#      fc::Board and fc::board() (bsp/include/fc/bsp/board.hpp)
#
# Vendor code (CubeMX/HAL/CMSIS) goes in bsp/<board>/vendor/ and is not clang-formatted.

set(FC_FREERTOS_PORT GCC_ARM_CM4F)

set(_cpu_flags -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
add_compile_options(${_cpu_flags})
add_link_options(${_cpu_flags} -T${CMAKE_CURRENT_LIST_DIR}/linker.ld)

add_library(fc_bsp STATIC
    ${CMAKE_CURRENT_LIST_DIR}/vendor/startup.s   # vector table, Reset_Handler (calls __libc_init_array)
    ${CMAKE_CURRENT_LIST_DIR}/vendor/system.c    # clock tree, SystemCoreClock
    ${CMAKE_CURRENT_LIST_DIR}/board.cpp)         # fc::Board implementation
target_include_directories(fc_bsp PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/vendor)
target_link_libraries(fc_bsp PUBLIC fc_core freertos_kernel)

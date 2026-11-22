# Board definition template. Copy this directory to bsp/<your_board>/ and fill it in.
#
# This file is include()d by flight-controller/CMakeLists.txt (same scope), so
# compile/link options set here apply to the whole firmware, including FreeRTOS.
#
# It must:
#   1. set FC_FREERTOS_PORT
#   2. set CPU flags with add_compile_options()/add_link_options()
#   3. define a static library target named `fc_bsp` implementing bsp/include/fc_bsp.h

set(FC_FREERTOS_PORT GCC_ARM_CM4F)

set(_cpu_flags -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
add_compile_options(${_cpu_flags})
add_link_options(${_cpu_flags} -T${CMAKE_CURRENT_LIST_DIR}/linker.ld)

add_library(fc_bsp STATIC
    ${CMAKE_CURRENT_LIST_DIR}/startup.s       # vector table, Reset_Handler
    ${CMAKE_CURRENT_LIST_DIR}/system.c        # clock tree, SystemCoreClock
    ${CMAKE_CURRENT_LIST_DIR}/bsp.c)          # implementation of fc_bsp.h
target_include_directories(fc_bsp PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(fc_bsp PUBLIC uav::common)

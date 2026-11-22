# SITL board: the whole firmware (FreeRTOS + app/) as a Linux process, using
# the FreeRTOS POSIX port. Sensors, RC and time come from the simulator over
# UDP (rfs_sim --udp); see sitl_board.cpp and scripts/sitl.sh.
#
# Built with the host compiler: scripts/build.sh --sitl

if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "bsp/sitl is built with the host compiler (scripts/build.sh --sitl)")
endif()

set(FC_FREERTOS_PORT GCC_POSIX)

# Seen by FreeRTOSConfig.h (kernel and application alike).
add_compile_definitions(RFS_FC_SITL)
# Same C++ subset as the MCU firmware (arm-none-eabi.cmake sets these there).
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions> $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

add_library(fc_bsp STATIC
    ${CMAKE_CURRENT_LIST_DIR}/sitl_board.cpp
    ${CMAKE_CURRENT_LIST_DIR}/udp_sim_backend.cpp)
target_include_directories(fc_bsp PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(fc_bsp PUBLIC fc_bsp_sim freertos_kernel)
rfs_set_warnings(fc_bsp)

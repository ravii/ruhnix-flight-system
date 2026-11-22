# Adds the FreeRTOS kernel from third_party/FreeRTOS-Kernel (git submodule).
#
#   rfs_add_freertos(
#       PORT       GCC_ARM_CM4F                 # FreeRTOS port name
#       HEAP       4                            # heap_N.c implementation
#       CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)   # dir with FreeRTOSConfig.h
#
# Provides the target `freertos_kernel`.

include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/RfsCommon.cmake)

function(rfs_add_freertos)
    cmake_parse_arguments(ARG "" "PORT;HEAP;CONFIG_DIR" "" ${ARGN})

    set(kernel_dir ${RFS_ROOT}/third_party/FreeRTOS-Kernel)
    if(NOT EXISTS ${kernel_dir}/CMakeLists.txt)
        message(FATAL_ERROR
            "FreeRTOS-Kernel not found at ${kernel_dir}.\n"
            "Run: git submodule update --init --recursive")
    endif()

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE ${ARG_CONFIG_DIR})

    set(FREERTOS_PORT ${ARG_PORT} CACHE STRING "FreeRTOS port" FORCE)
    set(FREERTOS_HEAP ${ARG_HEAP} CACHE STRING "FreeRTOS heap implementation" FORCE)

    add_subdirectory(${kernel_dir} ${CMAKE_BINARY_DIR}/_freertos)
endfunction()

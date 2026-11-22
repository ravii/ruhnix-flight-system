# Shared CMake helpers. Each module includes this file from its own CMakeLists.txt:
#
#   set(UAV_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
#   include(${UAV_ROOT}/cmake/UavCommon.cmake)

include_guard(GLOBAL)

get_filename_component(UAV_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Pull in the shared protocol library (target: uav::common).
function(uav_use_common)
    if(NOT TARGET uav_common)
        add_subdirectory(${UAV_ROOT}/common ${CMAKE_BINARY_DIR}/_uav_common)
    endif()
endfunction()

# Strict warnings for project-owned targets (not third-party code).
function(uav_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wdouble-promotion -Wundef)
    endif()
    if(UAV_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

option(UAV_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Shared CMake helpers. Each module includes this file from its own CMakeLists.txt:
#
#   set(RFS_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
#   include(${RFS_ROOT}/cmake/RfsCommon.cmake)

include_guard(GLOBAL)

get_filename_component(RFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Pull in the shared protocol library (target: rfs::common).
function(rfs_use_common)
    if(NOT TARGET rfs_common)
        add_subdirectory(${RFS_ROOT}/common ${CMAKE_BINARY_DIR}/_rfs_common)
    endif()
endfunction()

# Pull in the flight-controller algorithms (target: rfs::fc_core) without the
# rest of the firmware. Used by the simulator's in-process algorithm loop.
function(rfs_use_fc_core)
    if(NOT TARGET fc_core)
        add_subdirectory(${RFS_ROOT}/flight-controller/core ${CMAKE_BINARY_DIR}/_rfs_fc_core)
    endif()
endfunction()

# Strict warnings for project-owned targets (not third-party code).
function(rfs_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wdouble-promotion -Wundef)
    endif()
    if(RFS_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

option(RFS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# fc_core: the platform-independent flight-control algorithms (rfs::fc_core).
# No FreeRTOS, no hardware. Added by flight-controller/CMakeLists.txt, and by
# simulator/ (rfs_use_fc_core) for the in-process algorithm loop.

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/RfsCommon.cmake)
rfs_use_common()

add_library(fc_core STATIC
    src/esc/dshot.cpp
    src/esc/esc_output.cpp
    src/flight_control.cpp
    src/mixer.cpp
    src/pid.cpp)
add_library(rfs::fc_core ALIAS fc_core)
target_include_directories(fc_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../bsp/include)  # rfs::fc::Board interface; header-only
target_link_libraries(fc_core PUBLIC rfs::common)
rfs_set_warnings(fc_core)

cmake_minimum_required(VERSION 3.21)
project(uav_common LANGUAGES C)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/UavCommon.cmake)

add_library(uav_common STATIC
    protocol/src/frame.c)
add_library(uav::common ALIAS uav_common)

target_include_directories(uav_common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/protocol/include)
target_compile_features(uav_common PUBLIC c_std_11)
uav_set_warnings(uav_common)

# Tests are built only when common/ itself is the top-level project.
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(test_frame tests/test_frame.c)
        target_link_libraries(test_frame PRIVATE uav::common)
        add_test(NAME common.frame COMMAND test_frame)
    endif()
endif()

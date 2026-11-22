cmake_minimum_required(VERSION 3.21)
project(rfs_common LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/RfsCommon.cmake)

add_library(rfs_common STATIC
    src/frame.cpp)
add_library(rfs::common ALIAS rfs_common)

target_include_directories(rfs_common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_features(rfs_common PUBLIC cxx_std_17)
rfs_set_warnings(rfs_common)

# Tests are built only when common/ itself is the top-level project.
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(test_frame tests/test_frame.cpp)
        target_link_libraries(test_frame PRIVATE rfs::common)
        add_test(NAME common.frame COMMAND test_frame)
    endif()
endif()

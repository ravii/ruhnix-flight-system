cmake_minimum_required(VERSION 3.21)
project(rfs_localization VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/RfsCommon.cmake)
rfs_use_common()
find_package(Threads REQUIRED)

add_library(localization_lib STATIC
    src/pose_estimator.cpp)
target_include_directories(localization_lib PUBLIC include)
target_link_libraries(localization_lib PUBLIC rfs::common)
rfs_set_warnings(localization_lib)

add_executable(localization src/main.cpp)
target_link_libraries(localization PRIVATE localization_lib Threads::Threads)
rfs_set_warnings(localization)

install(TARGETS localization RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(localization_tests tests/test_pose_estimator.cpp)
    target_link_libraries(localization_tests PRIVATE localization_lib)
    add_test(NAME localization.pose_estimator COMMAND localization_tests)
endif()

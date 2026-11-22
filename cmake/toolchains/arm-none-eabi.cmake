# Toolchain for bare-metal / FreeRTOS ARM Cortex-M targets.
# CPU-specific flags (-mcpu, -mfpu, -mfloat-abi) are set by each board's BSP.
#
# Usage: -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake
# Optional: -DARM_TOOLCHAIN_DIR=/opt/arm-gnu-toolchain/bin

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(_prefix arm-none-eabi-)
if(ARM_TOOLCHAIN_DIR)
    set(_prefix ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-)
endif()

set(CMAKE_C_COMPILER   ${_prefix}gcc)
set(CMAKE_CXX_COMPILER ${_prefix}g++)
set(CMAKE_ASM_COMPILER ${_prefix}gcc)
set(CMAKE_OBJCOPY      ${_prefix}objcopy CACHE FILEPATH "")
set(CMAKE_SIZE         ${_prefix}size    CACHE FILEPATH "")

# No OS to link test executables against during compiler checks.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT   "-ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections --specs=nano.specs --specs=nosys.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

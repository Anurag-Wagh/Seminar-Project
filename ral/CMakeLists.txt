# CMakeLists.txt — OPAL SED Management for FreeRTOS
#
# Builds the complete OPAL stack for an embedded Cortex-M target.
# Also provides a host (desktop) test target using the POSIX RAL.
#
# ── Usage ──────────────────────────────────────────────────────────────────
#
# Embedded build (cross-compile for STM32):
#   cmake -B build_arm \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
#         -DTARGET_PLATFORM=stm32f407
#   cmake --build build_arm
#
# Desktop test build (runs on Linux/macOS with no hardware):
#   cmake -B build_host -DBUILD_HOST_TESTS=ON
#   cmake --build build_host
#   ./build_host/test_opal_core
#
# ── Project ─────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.16)
project(opal_freertos C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_HOST_TESTS  "Build desktop unit tests using POSIX RAL" OFF)
option(TARGET_PLATFORM   "Target platform: stm32f407, stm32h743, esp32" "stm32f407")

# ── Compiler warnings ────────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic)

# ── OPAL Core library (OS-agnostic, shared by both targets) ─────────────────
add_library(opal_core STATIC
    core/opal_core.c
)
target_include_directories(opal_core PUBLIC
    core/
    ral/
    transport/
)
target_compile_options(opal_core PRIVATE -DOPAL_LOG_LEVEL=3)

# ═════════════════════════════════════════════════════════════════════════════
# HOST TEST BUILD  (cmake -DBUILD_HOST_TESTS=ON)
# Uses POSIX RAL and mock transport — no FreeRTOS, no hardware needed
# ═════════════════════════════════════════════════════════════════════════════
if(BUILD_HOST_TESTS)
    add_executable(test_opal_core
        tests/test_opal_core.c
        transport/opal_transport_mock.c
        ral/opal_ral_posix.c
    )
    target_link_libraries(test_opal_core PRIVATE opal_core pthread)
    target_include_directories(test_opal_core PRIVATE
        core/ ral/ transport/ tests/
    )
    target_compile_options(test_opal_core PRIVATE -DOPAL_LOG_LEVEL=3)

    enable_testing()
    add_test(NAME opal_unit_tests COMMAND test_opal_core)
    message(STATUS "Host test build enabled — run: cmake --build . && ctest -V")
    return()   # stop here; don't configure embedded targets
endif()

# ═════════════════════════════════════════════════════════════════════════════
# EMBEDDED BUILD — FreeRTOS target
# ═════════════════════════════════════════════════════════════════════════════

# ── FreeRTOS source location ─────────────────────────────────────────────────
# Set FREERTOS_DIR to the root of your FreeRTOS source tree.
# Typically provided by your BSP/SDK or downloaded from:
#   https://github.com/FreeRTOS/FreeRTOS-Kernel
if(NOT DEFINED FREERTOS_DIR)
    set(FREERTOS_DIR "${CMAKE_SOURCE_DIR}/../FreeRTOS-Kernel"
        CACHE PATH "Path to FreeRTOS kernel source")
endif()

if(NOT EXISTS "${FREERTOS_DIR}/tasks.c")
    message(FATAL_ERROR
        "FreeRTOS not found at ${FREERTOS_DIR}.\n"
        "Clone it: git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git\n"
        "Or set -DFREERTOS_DIR=/path/to/FreeRTOS-Kernel")
endif()

# ── FreeRTOS kernel sources ───────────────────────────────────────────────────
set(FREERTOS_SRCS
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

# ── Cortex-M portable layer ───────────────────────────────────────────────────
# Adjust for your core: GCC/ARM_CM4F, GCC/ARM_CM7, GCC/ARM_CM3, etc.
set(FREERTOS_PORT_DIR "${FREERTOS_DIR}/portable/GCC/ARM_CM4F")
list(APPEND FREERTOS_SRCS "${FREERTOS_PORT_DIR}/port.c")

# ── Platform-specific transport source ───────────────────────────────────────
# Member 3 provides opal_transport_hw.c for the specific embedded platform.
set(TRANSPORT_HW_SRC "transport/opal_transport_hw.c")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${TRANSPORT_HW_SRC}")
    message(WARNING
        "${TRANSPORT_HW_SRC} not found. "
        "Member 3 must provide this file for the embedded build.")
    set(TRANSPORT_HW_SRC "transport/opal_transport_mock.c")  # fallback
endif()

# ── Main embedded executable ──────────────────────────────────────────────────
add_executable(opal_freertos.elf
    app/opal_app_freertos.c
    core/opal_core.c
    ral/opal_ral_freertos.c
    ${TRANSPORT_HW_SRC}
    ${FREERTOS_SRCS}
)

target_include_directories(opal_freertos.elf PRIVATE
    core/
    ral/
    transport/
    app/
    ${FREERTOS_DIR}/include
    ${FREERTOS_PORT_DIR}
)

target_compile_options(opal_freertos.elf PRIVATE
    -DOPAL_LOG_LEVEL=2          # INFO level for embedded (less verbose)
    -Os                         # size-optimise for flash constraints
    -ffunction-sections
    -fdata-sections
)

target_link_options(opal_freertos.elf PRIVATE
    -Wl,--gc-sections           # remove unused code sections
)

# ── Post-build: generate .bin and .hex for flashing ──────────────────────────
add_custom_command(TARGET opal_freertos.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary  opal_freertos.elf opal_freertos.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex    opal_freertos.elf opal_freertos.hex
    COMMENT "Generating opal_freertos.bin and opal_freertos.hex"
)

# ── Print size report ─────────────────────────────────────────────────────────
add_custom_command(TARGET opal_freertos.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} opal_freertos.elf
    COMMENT "Binary size:"
)

message(STATUS "Embedded build configured for: ${TARGET_PLATFORM}")
message(STATUS "FreeRTOS source: ${FREERTOS_DIR}")
message(STATUS "Transport: ${TRANSPORT_HW_SRC}")

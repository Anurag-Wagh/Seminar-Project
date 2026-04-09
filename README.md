# OPAL SED Management Project

This project ports the Linux kernel TCG OPAL implementation to an embedded FreeRTOS environment.

## Directory layout

- `core/` — OPAL protocol logic and public API (`opal_core.c`, `opal_core.h`, `opal_tokens.h`, `opal_uids.h`)
- `ral/` — RTOS abstraction layer and FreeRTOS implementation (`opal_ral.h`, `opal_ral_freertos.c`, `opal_ral_posix.c`)
- `transport/` — generic transport interface and mock/hardware transport stubs (`opal_transport.h`, `opal_transport_hw.c`, `opal_transport_hw.h`, `opal_transport_mock.c`, `opal_transport_mock.h`)
- `tests/` — desktop unit tests for the OPAL core
- `app/` — FreeRTOS application entry point (`opal_app_freertos.c`)
- `cmake/` — ARM GCC toolchain file for embedded builds

## Build options

### Desktop unit tests

This uses the POSIX RAL and mock transport only.

Requirements:
- `gcc`
- `make` or CMake
- `pthread` library

From terminal:

```bash
cd ~/Desktop/6th\ sem/Seminar\ Project
make test
```

Or with CMake:

```bash
cd ~/Desktop/6th\ sem/Seminar\ Project
cmake -B build_host -DBUILD_HOST_TESTS=ON
cmake --build build_host
./build_host/test_opal_core
```

### Embedded FreeRTOS build

Requirements:
- `arm-none-eabi-gcc`
- FreeRTOS Kernel source tree available at `../FreeRTOS-Kernel` or from `-DFREERTOS_DIR`

From terminal:

```bash
cd ~/Desktop/6th\ sem/Seminar\ Project
make embedded
```

Or with CMake:

```bash
cd ~/Desktop/6th\ sem/Seminar\ Project
cmake -B build_arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DFREERTOS_DIR="/path/to/FreeRTOS-Kernel"
cmake --build build_arm
```

## Important notes

- `opal_ral_posix.c` is only for desktop testing.
- `opal_ral_freertos.c` is the real FreeRTOS implementation.
- `transport/opal_transport_hw.c` is currently a stub. Replace it with the actual ATA/NVMe transport for your hardware.
- `FreeRTOSConfig.h` in `ral/` is a template. Adjust it for your MCU and BSP.

## What is complete

- OPAL core extraction (Member 1) is implemented in `core/`.
- FreeRTOS RAL implementation is present in `ral/opal_ral_freertos.c`.
- Embedded app entry point is in `app/opal_app_freertos.c`.
- Hardware transport stub is in `transport/opal_transport_hw.c`.
- Root build files: `Makefile`, `CMakeLists.txt`, `cmake/arm-none-eabi.cmake`.

## Remaining work

- Implement the real `opal_transport_hw.c` for your specific SED hardware.
- Enable UART or platform logging for `opal_ral_freertos.c` via `configPRINTF` or `opal_platform_log_write`.
- Test on actual hardware and capture serial logs.

# OPAL SED Management Project

A FreeRTOS port of the OPAL drive management core.

## Project files

- `core/` — protocol logic and public API
- `ral/` — OS abstraction and FreeRTOS mapping
- `transport/` — transport interface plus mock/hardware backends
- `tests/` — desktop unit tests
- `app/` — FreeRTOS main task
- `cmake/` — ARM toolchain support

## Build commands

```bash
make test
make embedded
make check
```

## Notes

- `make test` builds a desktop test binary.
- `make embedded` builds FreeRTOS firmware.
- `transport/opal_transport_hw.c` is a placeholder for real hardware transport.
- Update `ral/FreeRTOSConfig.h` for your MCU if needed.

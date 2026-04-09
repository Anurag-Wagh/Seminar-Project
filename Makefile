# OPAL SED project build file
# make test      — desktop unit tests using POSIX RAL and mock transport
# make embedded  — cross-compile firmware with FreeRTOS support
# make check     — portability audit for core code

CC_HOST  = gcc
CC_ARM   = arm-none-eabi-gcc

WARN     = -Wall -Wextra -Wpedantic
CSTD     = -std=c99
IFLAGS   = -I.

CFLAGS_HOST = $(WARN) $(CSTD) $(IFLAGS) -g -DOPAL_LOG_LEVEL=3
LDFLAGS_HOST = -lpthread

CFLAGS_ARM = $(WARN) $(CSTD) $(IFLAGS) \
             -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
             -Os -ffunction-sections -fdata-sections -DOPAL_LOG_LEVEL=2
LDFLAGS_ARM = -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs

FREERTOS_DIR ?= ../FreeRTOS-Kernel

SRC_CORE      = core/opal_core.c
SRC_RAL_POSIX = ral/opal_ral_posix.c
SRC_RAL_RTOS  = ral/opal_ral_freertos.c
SRC_MOCK      = transport/opal_transport_mock.c
SRC_TEST      = tests/test_opal_core.c
SRC_APP       = app/opal_app_freertos.c
SRC_HW        = transport/opal_transport_hw.c

SRC_FRT = $(FREERTOS_DIR)/tasks.c \
          $(FREERTOS_DIR)/queue.c \
          $(FREERTOS_DIR)/list.c \
          $(FREERTOS_DIR)/timers.c \
          $(FREERTOS_DIR)/portable/MemMang/heap_4.c \
          $(FREERTOS_DIR)/portable/GCC/ARM_CM4F/port.c

IFLAGS_ARM = -Icore -Iral -Itransport -Iapp -Ifreertos_port \
             -I$(FREERTOS_DIR)/include \
             -I$(FREERTOS_DIR)/portable/GCC/ARM_CM4F

BIN_TEST = tests/test_opal_core
BIN_ARM  = opal_freertos.elf

.PHONY: all test embedded check clean help
all: test

$(BIN_TEST): $(SRC_TEST) $(SRC_CORE) $(SRC_RAL_POSIX) $(SRC_MOCK)
	$(CC_HOST) $(CFLAGS_HOST) $^ -o $@ $(LDFLAGS_HOST)
	@echo "  Build OK → $(BIN_TEST)  [RAL: POSIX — testing only]"

test: $(BIN_TEST)
	@echo ""
	./$(BIN_TEST)
	@echo ""

embedded:
	$(CC_ARM) $(CFLAGS_ARM) $(IFLAGS_ARM) \
	    $(SRC_CORE) $(SRC_RAL_RTOS) $(SRC_APP) $(SRC_HW) $(SRC_FRT) \
	    -o $(BIN_ARM) $(LDFLAGS_ARM)
	arm-none-eabi-size $(BIN_ARM)
	arm-none-eabi-objcopy -O binary $(BIN_ARM) opal_freertos.bin
	arm-none-eabi-objcopy -O ihex   $(BIN_ARM) opal_freertos.hex
	@echo "  Build OK → $(BIN_ARM)  [RAL: FreeRTOS — real target]"

check:
	@echo "Portability audit..."
	@grep -rn "^#include <linux/" core/ && echo "FAIL" && exit 1 || echo "  OK: no Linux headers"
	@grep -rn "^#include <freertos\|^#include <FreeRTOS" core/ && echo "FAIL" && exit 1 || echo "  OK: no FreeRTOS headers"
	@echo "  Portability check PASSED"

clean:
	rm -f $(BIN_TEST) $(BIN_ARM) *.bin *.hex *.o core/*.o ral/*.o transport/*.o app/*.o tests/*.o

help:
	@echo "make test      — desktop unit tests (POSIX RAL)"
	@echo "make embedded  — cross-compile for FreeRTOS (arm-none-eabi-gcc)"
	@echo "make check     — portability audit"
	@echo "make clean     — remove build artifacts"

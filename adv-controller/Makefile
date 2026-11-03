# =============================================================================
# ADV Controller Firmware - ARM GCC Makefile (macOS/Linux/Windows)
# HC32F460 + ADV7280/ADV7391 Video Processor Controller
# =============================================================================

# A failed recipe deletes its target
.DELETE_ON_ERROR:

# Project name
PROJECT = adv-controller

# Build configuration: debug or release
BUILD ?= release

# Firmware version (link v3 image header and INFO)
VERSION := $(shell cat VERSION)

# Toolchain prefix
CROSS_COMPILE ?= arm-none-eabi-

# Tools
CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE    = $(CROSS_COMPILE)size
PYTHON ?= python3

# =============================================================================
# Directories
# =============================================================================
BUILD_DIR = build/$(BUILD)
SRC_DIR   = src
LIB_DIR   = lib

# =============================================================================
# Source files
# =============================================================================

C_SOURCES = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/i2c.c \
    $(SRC_DIR)/uart_dma.c \
    $(SRC_DIR)/adv7280.c \
    $(SRC_DIR)/adv7391.c \
    $(SRC_DIR)/board.c \
    $(SRC_DIR)/led.c \
    $(SRC_DIR)/signal.c \
    $(SRC_DIR)/debug_uart.c \
    $(SRC_DIR)/console.c \
    $(SRC_DIR)/console_hc32.c \
    $(SRC_DIR)/console_fmt.c \
    $(SRC_DIR)/cdc_ring.c \
    $(SRC_DIR)/usb_guard.c \
    $(SRC_DIR)/usb_bsp.c \
    $(SRC_DIR)/usb_dev_desc.c \
    $(SRC_DIR)/usb_cdc.c \
    $(SRC_DIR)/usb_console.c \
    $(LIB_DIR)/usb_device_lib/device_core/usb_dev_core.c \
    $(LIB_DIR)/usb_device_lib/device_core/usb_dev_ctrleptrans.c \
    $(LIB_DIR)/usb_device_lib/device_core/usb_dev_driver.c \
    $(LIB_DIR)/usb_device_lib/device_core/usb_dev_int.c \
    $(LIB_DIR)/usb_device_lib/device_core/usb_dev_stdreq.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_usb.c \
    $(SRC_DIR)/link.c \
    $(SRC_DIR)/link_hal_hc32.c \
    $(SRC_DIR)/link_hal_adv.c \
    ../shared/adv-link/adv-link.c \
    $(SRC_DIR)/fault_handler.c \
    $(SRC_DIR)/fw_header.c \
    $(SRC_DIR)/fw_copy.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_clk.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_efm.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_gpio.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_interrupts.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_pwc.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_sram.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_usart.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_utility.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_crc.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_fcg.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_tmr0.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_i2c.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_fcm.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_icg.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_aos.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_dma.c \
    $(LIB_DIR)/hc32_ll_driver/src/hc32_ll_wdt.c \
    $(LIB_DIR)/bsp/ev_hc32f460_lqfp100_v2/ev_hc32f460_lqfp100_v2.c \
    $(LIB_DIR)/cmsis/Device/HDSC/hc32f4xx/Source/system_hc32f460.c

ASM_SOURCES = \
    $(LIB_DIR)/cmsis/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S

# =============================================================================
# Include paths
# =============================================================================
INCLUDES = \
    -I$(SRC_DIR) \
    -I../shared/adv-link \
    -I$(LIB_DIR)/cmsis/Device/HDSC/hc32f4xx/Include \
    -I$(LIB_DIR)/cmsis/Include \
    -I$(LIB_DIR)/hc32_ll_driver/inc \
    -I$(LIB_DIR)/bsp/ev_hc32f460_lqfp100_v2 \
    -I$(LIB_DIR)/usb_device_lib \
    -I$(LIB_DIR)/usb_device_lib/device_core

# =============================================================================
# Compiler/Linker flags
# =============================================================================

CPU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS = $(CPU_FLAGS)
CFLAGS += -Wall
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -std=c99
CFLAGS += $(INCLUDES)
CFLAGS += -DHC32F460
CFLAGS += -DUSE_DDL_DRIVER
CFLAGS += -DADV_FW_VERSION=\"$(VERSION)\"
CFLAGS += -MMD -MP   # header dependencies (.d beside each .o): a changed .h rebuilds its users

ifeq ($(BUILD),debug)
    CFLAGS += -g3 -gdwarf-2 -O0
    CFLAGS += -D__DEBUG
else
    CFLAGS += -Os
endif

ASFLAGS = $(CPU_FLAGS)
ASFLAGS += -x assembler-with-cpp
ASFLAGS += $(INCLUDES)

LDSCRIPT = linker/hc32f460-app.ld

LDFLAGS = $(CPU_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map,--cref
LDFLAGS += --specs=nano.specs
LDFLAGS += --specs=nosys.specs
LDFLAGS += -lc -lm -lnosys

# =============================================================================
# Object files - flatten to build directory
# =============================================================================
C_OBJECTS   = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.S=.o)))
OBJECTS     = $(C_OBJECTS) $(ASM_OBJECTS)

# VPATH tells make where to find source files
VPATH = $(SRC_DIR):$(LIB_DIR)/usb_device_lib/device_core:../shared/adv-link:$(LIB_DIR)/hc32_ll_driver/src:$(LIB_DIR)/bsp/ev_hc32f460_lqfp100_v2:$(LIB_DIR)/cmsis/Device/HDSC/hc32f4xx/Source:$(LIB_DIR)/cmsis/Device/HDSC/hc32f4xx/Source/GCC

# =============================================================================
# Build rules
# =============================================================================

all: $(BUILD_DIR) $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin $(BUILD_DIR)/$(PROJECT).hex
	@echo ""
	@echo "Build complete: $(BUILD)"
	@$(SIZE) $(BUILD_DIR)/$(PROJECT).elf

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: %.c
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@echo "AS  $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS)
	@echo "LD  $@"
	@$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	@"$(PYTHON)" -I tools/check-ramfunc.py $(BUILD_DIR)/fw_copy.o $@

# Patched in a temporary file: a failed patch never leaves an unpatched .bin that looks up to date
$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	@echo "BIN $@"
	@$(OBJCOPY) -O binary $< $@.tmp
	@"$(PYTHON)" -I tools/fw-header.py --patch $@.tmp
	@mv $@.tmp $@

# From the patched .bin, so the header length and CRC32 are in the .hex too
$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).bin
	@echo "HEX $@"
	@$(OBJCOPY) -I binary -O ihex --change-addresses 0x10000 $< $@

clean:
	@echo "Cleaning..."
	@rm -rf build

debug:
	@$(MAKE) BUILD=debug

# Release build, copied to release/ (the image the ESP firmware embeds)
release:
	@$(MAKE) BUILD=release
	@mkdir -p release
	@cp build/release/$(PROJECT).bin release/$(PROJECT).bin
	@"$(PYTHON)" -I tools/fw-header.py --check release/$(PROJECT).bin

size: $(BUILD_DIR)/$(PROJECT).elf
	@$(SIZE) --format=berkeley $<

disasm: $(BUILD_DIR)/$(PROJECT).elf
	@$(OBJDUMP) -d -S $< > $(BUILD_DIR)/$(PROJECT).dis

# Host tests of the link v3 dispatcher (host compiler, stub HAL; no DDL)
HOSTCC ?= cc
HOST_BUILD = build/host
test:
	@mkdir -p $(HOST_BUILD)
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) -I../shared/adv-link -Itest \
		test/test_link.c test/stub_hal.c $(SRC_DIR)/link.c ../shared/adv-link/adv-link.c -o $(HOST_BUILD)/test_link
	@$(HOST_BUILD)/test_link
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) test/test_console.c $(SRC_DIR)/console.c -o $(HOST_BUILD)/test_console
	@$(HOST_BUILD)/test_console
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) test/test_cdc_ring.c $(SRC_DIR)/cdc_ring.c -o $(HOST_BUILD)/test_cdc_ring
	@$(HOST_BUILD)/test_cdc_ring
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) -I../shared/adv-link test/test_console_fmt.c $(SRC_DIR)/console_fmt.c -o $(HOST_BUILD)/test_console_fmt
	@$(HOST_BUILD)/test_console_fmt
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) test/test_usb_guard.c $(SRC_DIR)/usb_guard.c -o $(HOST_BUILD)/test_usb_guard
	@$(HOST_BUILD)/test_usb_guard
	@"$(PYTHON)" -I tools/test_check_ramfunc.py
	@$(HOSTCC) -std=c99 -Wall -Wextra -I$(SRC_DIR) -I../shared/adv-link test/test_signal.c $(SRC_DIR)/signal.c ../shared/adv-link/adv-link.c -o $(HOST_BUILD)/test_signal
	@$(HOST_BUILD)/test_signal
	@$(HOSTCC) -std=c99 -Wall -Wextra -Wno-unused-parameter -Itest/shim -I$(SRC_DIR) -I../shared/adv-link -DGOLDEN_HOST \
		-DI2C_HOST_TEST test/test_i2c.c $(SRC_DIR)/i2c.c -o $(HOST_BUILD)/test_i2c
	@$(HOST_BUILD)/test_i2c
	@$(MAKE) --no-print-directory golden

# Golden I2C traces: the firmware's own ADV code on the host with DDL shims (test/shim); GOLDEN_UPDATE=1 rewrites
golden:
	@mkdir -p $(HOST_BUILD)
	@$(HOSTCC) -std=c99 -Wall -Wextra -Wno-unused-parameter -Itest/shim -I$(SRC_DIR) -I../shared/adv-link -Itest \
		-DGOLDEN_HOST test/test_golden.c test/golden_trace.c $(SRC_DIR)/adv7280.c $(SRC_DIR)/adv7391.c $(SRC_DIR)/board.c \
		$(SRC_DIR)/led.c $(SRC_DIR)/signal.c $(SRC_DIR)/link_hal_adv.c $(SRC_DIR)/link.c \
		../shared/adv-link/adv-link.c -o $(HOST_BUILD)/test_golden
	@$(HOST_BUILD)/test_golden

help:
	@echo "ADV Controller Firmware Build System"
	@echo ""
	@echo "Usage: make [target] [BUILD=debug|release]"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build firmware (default)"
	@echo "  debug    - Build debug version"
	@echo "  release  - Build release version"
	@echo "  clean    - Remove build files"
	@echo "  size     - Print firmware size"
	@echo "  disasm   - Generate disassembly"
	@echo "  help     - Print this help"

.PHONY: all clean debug release size disasm help test golden

# Header dependencies, last so a rule inside a .d file never becomes the default goal
-include $(C_OBJECTS:.o=.d)

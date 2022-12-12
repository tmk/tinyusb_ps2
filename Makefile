#
# TinyUSB build for Raspberry Pi Pico RP2040
#

# top directory of tinyusb
TINYUSB_PATH = ../tinyusb

PICO_SDK_PATH = ../pico-sdk


FAMILY := rp2040
BOARD := pico_sdk

# Build directory
BUILD := _build

PROJECT := $(notdir $(CURDIR))




#
# import from hw/bsp/rp/2040/family.mk
#
DEPS_SUBMODULES += hw/mcu/raspberry_pi/Pico-PIO-USB

ifeq ($(DEBUG), 1)
CMAKE_DEFSYM += -DCMAKE_BUILD_TYPE=Debug
endif

$(BUILD):
	cmake -S . -B $(BUILD) -DTINYUSB_PATH=$(TINYUSB_PATH) -DPICO_SDK_PATH=$(PICO_SDK_PATH) -DFAMILY=$(FAMILY) -DBOARD=$(BOARD) -DPICO_BUILD_DOCS=0 $(CMAKE_DEFSYM)

all: $(BUILD)
	make -C $(BUILD)

clean:
	rm -rf $(BUILD)

flash:
	picotool load $(BUILD)/$(PROJECT).uf2
	picotool reboot




#
# import from examples/rules.mk
#
# Set all as default goal
.DEFAULT_GOAL := all

# get depenecies
.PHONY: get-deps
get-deps:
ifdef DEPS_SUBMODULES
	git -C $(TINYUSB_PATH) submodule update --init $(DEPS_SUBMODULES)
endif

.PHONY: size
size: $(BUILD)/$(PROJECT).elf
	-@echo ''
	@arm-none-eabi-size $<
	-@echo ''

# linkermap must be install previously at https://github.com/hathach/linkermap
linkermap: $(BUILD)/$(PROJECT).elf
	@linkermap -v $<.map

# Print out the value of a make variable.
# https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%:
	@echo $* = $($*)

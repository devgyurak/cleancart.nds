# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Build of the probes: demo ROMs that report what an emulator does for
# behaviour nobody has confirmed on hardware (L4). Run through ./build.sh to use
# the pinned container.
#
#   make                  every probe ROM listed in DEMOS
#   make DEMO=<name>      one probe ROM            -> probes-<name>.nds
#   make release          build/release/{demos,judge}
#   make clean

BLOCKSDS            ?= /opt/wonderful/thirdparty/blocksds/core
WONDERFUL_TOOLCHAIN ?= /opt/wonderful
PREFIX              := $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-
CC                  := $(PREFIX)gcc
PYTHON              ?= python3
NDSTOOL             := $(BLOCKSDS)/tools/ndstool/ndstool

DEMO   ?=

GAME_TITLE    := Probes
GAME_SUBTITLE := Research probes
GAME_AUTHOR   := devgyurak
# Header game code, maker code and title. Not "####": melonDS (and some
# loaders) treat that code as homebrew and attach no save chip at all. PRBS
# is used by no other game here and is in no ROM list we know of (melonDS's
# src/ROMList.cpp), so melonDS emulates its default 8 KB EEPROM.
GAME_CODE     := PRBS 00 PROBES
# DS-only unit code (see games/corvane/Makefile for why).
UNIT_CODE     := 0

# The ROM names. The ROM demo names carry the l4- prefix so that their
# result-block ids differ from the same demos' ids in Corvane.
DEMOS := \
  l4-sys-dma-gfx l4-sys-dma-disp l4-2d-obj-line l4-2d-display \
  l4-3d-fifo l4-3d-matrix l4-3d-light l4-3d-clip l4-3d-poly l4-3d-test \
  l4-acc-stage l4-acc-dual l4-acc-split-rear l4-acc-split-hdma l4-acc-rear-plane

VARIANT  := probes-$(DEMO)
ROM      := $(VARIANT).nds
OUT      := build/$(VARIANT)
GEN      := build/gen
NITRO    := build/nitrofs
STAMP    := $(GEN)/.stamp

# Compiler flags shared by both CPUs. The save code is linked for its state
# struct only; the probes never read or write the cartridge save.
DEFINES  := -D__NDS__ -D__BLOCKSDS__ -DCV_SAVE_TYPE=1
ifneq ($(DEMO),)
  DEFINES += -DCV_DEMO=\"$(DEMO)\"
endif

INCS     := -Icommon -I$(GEN) -I$(BLOCKSDS)/libs/libnds/include
WARN     := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
OPT      := -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing

ARCH9    := -mthumb -mcpu=arm946e-s+nofp
ARCH7    := -mthumb -mcpu=arm7tdmi
SPECS9   := $(BLOCKSDS)/sys/crts/ds_arm9.specs
SPECS7   := $(BLOCKSDS)/sys/crts/ds_arm7.specs

CFLAGS9  := $(WARN) $(OPT) $(ARCH9) -Iarm9/source $(INCS) $(DEFINES) -DARM9 -specs=$(SPECS9)
CFLAGS7  := $(WARN) $(OPT) $(ARCH7) -Iarm7/source $(INCS) $(DEFINES) -DARM7 -specs=$(SPECS7)

# The ARM7 core does not live in shared WRAM: the ARM9 hands those 32 KB
# back and forth between the CPUs (loading scratch).
LDFLAGS9 := $(ARCH9) -specs=$(SPECS9) -L$(BLOCKSDS)/libs/libnds/lib \
            -Wl,--gc-sections -Wl,-Map,$(OUT)/arm9.map
LDFLAGS7 := $(ARCH7) -specs=$(SPECS7) -L$(BLOCKSDS)/libs/libnds/lib \
            -Wl,--gc-sections -Wl,-Map,$(OUT)/arm7.map \
            -Wl,--defsym=__shared_wram_size=0

SRC9 := $(shell find arm9/source -name '*.c' | LC_ALL=C sort)
SRC7 := $(shell find arm7/source -name '*.c' | LC_ALL=C sort)
OBJ9 := $(SRC9:%.c=$(OUT)/9/%.o) $(OUT)/9/gen/assets.o
OBJ7 := $(SRC7:%.c=$(OUT)/7/%.o)

.PHONY: all clean demos assets release
ifeq ($(DEMO),)
all: demos
else
all: $(ROM)
endif

assets: $(STAMP)

# Build-time asset tools (Python, stdlib only). Everything they emit is
# generated from source code in tools/; see ASSETS.md.
$(STAMP): $(wildcard tools/*.py tools/cvtools/*.py licenses/*) LICENSE THIRD_PARTY_NOTICES
	@echo "  ASSETS  tools/mkassets.py"
	@mkdir -p $(GEN) $(NITRO)/licenses
	@$(PYTHON) tools/mkassets.py $(GEN) $(NITRO)
	@cp licenses/* LICENSE THIRD_PARTY_NOTICES $(NITRO)/licenses/
	@touch $@

$(GEN)/assets.c $(GEN)/assets.h $(GEN)/icon.bmp: $(STAMP)

$(OUT)/9/%.o: %.c $(STAMP)
	@echo "  CC.9    $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS9) -MMD -MP -c -o $@ $<

$(OUT)/9/gen/assets.o: $(GEN)/assets.c
	@echo "  CC.9    $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS9) -c -o $@ $<

$(OUT)/7/%.o: %.c $(STAMP)
	@echo "  CC.7    $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS7) -MMD -MP -c -o $@ $<

$(OUT)/arm9.elf: $(OBJ9)
	@echo "  LD.9    $@"
	@$(CC) -o $@ $(OBJ9) $(LDFLAGS9) -Wl,--start-group -lnds9 -lc -Wl,--end-group

$(OUT)/arm7.elf: $(OBJ7)
	@echo "  LD.7    $@"
	@$(CC) -o $@ $(OBJ7) $(LDFLAGS7) -Wl,--start-group -lnds7 -lc -Wl,--end-group

$(ROM): $(OUT)/arm9.elf $(OUT)/arm7.elf $(STAMP)
	@echo "  NDSTOOL $@"
	@$(NDSTOOL) -c $@ -7 $(OUT)/arm7.elf -9 $(OUT)/arm9.elf -g $(GAME_CODE) -uc $(UNIT_CODE) \
		-b $(GEN)/icon.bmp "$(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)" \
		-d $(NITRO)

demos:
	@for d in $(DEMOS); do $(MAKE) --no-print-directory DEMO=$$d || exit 1; done

# Files for a tagged release (.github/workflows/release.yml uploads them):
# build/release/demos (the probe ROMs) and build/release/judge (expected.json +
# judge.py, which records and compares what an emulator does), each with the
# licence texts and notices a binary distribution has to carry.
RELEASE := build/release

release:
	@rm -rf $(RELEASE)
	@$(MAKE) --no-print-directory demos
	@echo "  RELEASE $(RELEASE)"
	@mkdir -p $(RELEASE)/demos && \
		cp -R LICENSE NOTICE THIRD_PARTY_NOTICES licenses $(RELEASE)/demos/
	@for d in $(DEMOS); do cp probes-$$d.nds $(RELEASE)/demos/ || exit 1; done
	@mkdir -p $(RELEASE)/judge && \
		cp -R LICENSE NOTICE THIRD_PARTY_NOTICES licenses expected.json tools/judge.py $(RELEASE)/judge/

clean:
	@echo "  CLEAN"
	@rm -rf build probes*.nds

-include $(OBJ9:.o=.d) $(OBJ7:.o=.d)

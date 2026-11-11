# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Hushwell build. Run through ./build.sh to use the pinned container.
#
#   make                  main game ROM            -> hushwell.nds
#   make DEMO=<name>      single-feature demo ROM  -> hushwell-<name>.nds
#   make SAVE=<type>      save-chip variant (eeprom | flash | fram)
#   make demos            every demo ROM listed in DEMOS
#   make clean

BLOCKSDS            ?= /opt/wonderful/thirdparty/blocksds/core
WONDERFUL_TOOLCHAIN ?= /opt/wonderful
PREFIX              := $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-
CC                  := $(PREFIX)gcc
PYTHON              ?= python3
NDSTOOL             := $(BLOCKSDS)/tools/ndstool/ndstool

DEMO   ?=
SAVE   ?= eeprom

# The save-chip demos build against their own chip type.
ifeq ($(DEMO),io-save-flash)
  override SAVE := flash
endif
ifeq ($(DEMO),io-save-fram)
  override SAVE := fram
endif

GAME_TITLE    := Hushwell
GAME_SUBTITLE := A survival horror under the well
GAME_AUTHOR   := Corvane contributors
# Header game code, maker code and title. Not "####": melonDS (and some
# loaders) treat that code as homebrew and attach no save chip at all. HSWL
# is in no ROM list we know of (corvane uses CRVN); melonDS then emulates an
# 8 KB EEPROM.
GAME_CODE     := HSWL 00 HUSHWELL
# DS-only unit code. With the default (2, DS or DSi) and no DSi region,
# melonDS blocks every card read of a non-homebrew ROM in DS mode as "DSi
# area", and NitroFS fails to mount. Hushwell uses no DSi features.
UNIT_CODE     := 0

ifeq ($(DEMO),)
  ifeq ($(SAVE),eeprom)
    VARIANT := hushwell
  else
    VARIANT := hushwell-save-$(SAVE)
  endif
else
  VARIANT := hushwell-$(DEMO)
endif

ROM      := $(VARIANT).nds
OUT      := build/$(VARIANT)
GEN      := build/gen
NITRO    := build/nitrofs
STAMP    := $(GEN)/.stamp

# Every demo build name from the design document's hardware tables.
DEMOS := \
  sys-ipc sys-wram sys-irq sys-timer \
  sys-dma-now sys-dma-vblank sys-dma-hblank sys-dma-cart sys-dma-gfx \
  sys-dma-disp sys-dma-fill sys-bios sys-math sys-keys \
  2d-engines 2d-text 2d-affine 2d-bitmap 2d-mosaic \
  2d-obj-normal 2d-obj-affine 2d-obj-bitmap 2d-obj-map2d 2d-obj-line \
  2d-extpal 2d-window 2d-blend 2d-brightness 2d-vram 2d-capture 2d-display \
  3d-fifo 3d-matrix 3d-light 3d-clip \
  3d-tex-a3i5 3d-tex-a5i3 3d-tex-pal4 3d-tex-pal16 3d-tex-pal256 \
  3d-tex-4x4 3d-tex-direct \
  3d-poly 3d-toon 3d-edge 3d-fog 3d-aa 3d-depth 3d-test 3d-compose \
  3d-limits \
  snd-mix snd-pcm snd-adpcm snd-psg snd-capture snd-stream \
  io-cart io-save-eeprom io-save-flash io-save-fram io-firmware io-rtc \
  io-touch io-power

# Compiler flags shared by both CPUs.
SAVE_DEF := $(if $(filter flash,$(SAVE)),2,$(if $(filter fram,$(SAVE)),3,1))
DEFINES  := -D__NDS__ -D__BLOCKSDS__ -DCV_SAVE_TYPE=$(SAVE_DEF)
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

# The ARM7 core does not live in shared WRAM: the game hands those 32 KB
# back and forth between the CPUs (stream buffer / loading scratch).
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
all: $(ROM)

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
# build/release/rom (game + save variants) and build/release/demos, each
# with the licence texts and notices a binary distribution has to carry.
RELEASE := build/release

release:
	@rm -rf $(RELEASE)
	@$(MAKE) --no-print-directory
	@$(MAKE) --no-print-directory SAVE=flash
	@$(MAKE) --no-print-directory SAVE=fram
	@$(MAKE) --no-print-directory demos
	@echo "  RELEASE $(RELEASE)"
	@for d in rom demos; do \
		mkdir -p $(RELEASE)/$$d && \
		cp -R LICENSE NOTICE THIRD_PARTY_NOTICES licenses $(RELEASE)/$$d/ || exit 1; done
	@cp hushwell.nds hushwell-save-flash.nds hushwell-save-fram.nds $(RELEASE)/rom/
	@for d in $(DEMOS); do cp hushwell-$$d.nds $(RELEASE)/demos/ || exit 1; done

clean:
	@echo "  CLEAN"
	@rm -rf build hushwell*.nds

-include $(OBJ9:.o=.d) $(OBJ7:.o=.d)

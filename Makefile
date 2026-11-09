# ETCH -- A Generative Acid Machine -- by Synth Centric
# Reproducible build.  Requires GBDK-2020 4.3.0 (set GBDK_HOME) and Python 3
# with numpy (wavetable generation).  See README.md.

GBDK_HOME ?= /opt/gbdk
LCC       := $(GBDK_HOME)/bin/lcc
PYTHON    ?= python3
HOSTCC    ?= cc

ROMNAME   := ETCH_by_Synth_Centric
BUILD     := build
RELEASE   := release

# MBC5 + RAM + BATTERY (0x1B), 128 KB ROM (8 banks), 32 KB SRAM (4 banks).
# The CGB flag stays 0x00: ETCH is a DMG ROM that also runs on Game Boy
# Color in its DMG compatibility mode.
LDFLAGS   := -Wl-yt0x1B -Wl-yo8 -Wl-ya4 -autobank -Wb-ext=.rel \
             -Wm-yn"ETCH" -Wm-yk"SC" -Wm-yl0x33 -Wl-m -Wl-j
CFLAGS    := -Iinclude -Wa-l
# the interrupt-time code gets SDCC's expensive register allocator
HOTFLAGS  := -Wf--max-allocs-per-node20000 -Wf--opt-code-speed
HOTSRCS   := src/audio/engine.c src/sequencer/seq.c src/drums/drums.c

GEN_WAVES := assets/generated_waves/wavetables.c assets/generated_waves/wavelut.c
GEN_GFX   := src/ui/gfx_data.c include/gfx_tiles.h
GEN_PRE   := src/presets/preset_data.c src/presets/track_names.c

SRCS := $(sort $(wildcard src/*.c src/*/*.c) $(GEN_WAVES) src/ui/gfx_data.c $(GEN_PRE))
ASMS := $(sort $(wildcard src/*/*.s))
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS)) $(patsubst %.s,$(BUILD)/%.o,$(ASMS))
HDRS := $(wildcard include/*.h) include/gfx_tiles.h

.PHONY: all clean rom test hosttest emutest release distclean

all: rom

rom: $(BUILD)/$(ROMNAME).gb

$(GEN_WAVES): tools/generate_waves.py
	$(PYTHON) tools/generate_waves.py assets/generated_waves

$(GEN_GFX): tools/gen_gfx.py
	$(PYTHON) tools/gen_gfx.py src/ui/gfx_data.c include/gfx_tiles.h

src/audio/engine_tables.c: tools/gen_engine_tables.py
	$(PYTHON) tools/gen_engine_tables.py $@

$(GEN_PRE): tools/presets.py
	$(PYTHON) tools/presets.py src/presets/preset_data.c

$(BUILD)/%.o: %.c $(HDRS) | $(GEN_GFX)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) $(if $(filter $<,$(HOTSRCS)),$(HOTFLAGS)) -c -o $@ $<

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(LCC) -c -o $@ $<

$(BUILD)/$(ROMNAME).gb: $(OBJS)
	$(LCC) $(LDFLAGS) -o $@ $(OBJS)
	$(PYTHON) tools/check_rom.py $@

# ---- tests --------------------------------------------------------------
HOST_SRCS := src/generator/corrosion.c src/generator/pattern.c src/generator/names.c tests/test_generator.c

$(BUILD)/test_generator: $(HOST_SRCS) include/etch.h include/corrosion.h
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c99 -O1 -Wall -Wextra -Iinclude -o $@ $(HOST_SRCS)

hosttest: $(BUILD)/test_generator
	./$(BUILD)/test_generator

emutest: rom
	$(PYTHON) tests/emu_test.py $(BUILD)/$(ROMNAME).gb

test: hosttest emutest

# ---- release ------------------------------------------------------------
release: rom hosttest
	@mkdir -p $(RELEASE)
	cp $(BUILD)/$(ROMNAME).gb $(RELEASE)/$(ROMNAME).gb
	$(PYTHON) tools/make_source_zip.py $(RELEASE)/ETCH_Source.zip

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -f $(RELEASE)/*.gb $(RELEASE)/*.zip

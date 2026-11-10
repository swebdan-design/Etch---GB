# Third-Party Notices

**ETCH — A Generative Acid Machine**
by Synth Centric

## Linked into the ROM

### GBDK-2020 runtime library (version 4.3.0)

- <https://github.com/gbdk-2020/gbdk-2020>
- The ROM links the GBDK-2020 startup code and library functions (crt0, interrupt dispatcher, `joypad`, `set_bkg_data`, `set_bkg_tiles`, `memcpy`, banking trampolines and so on).
- **Licence:** GNU GPL v2 **with the GBDK linking exception**: *"if you link this library with other files, to produce an executable, this library does not by itself cause the resulting executable to be covered by the GNU General Public License."* The full text ships with GBDK-2020 in `licenses/LICENSE_GPLV2_LE`.
- The SDCC run-time support routines included by the compiler (integer multiply/divide helpers) are GPL with the same style of linking exception (SDCC run-time libraries, "GPL+LE").

## Build tools (not distributed with ETCH)

| Tool | Licence | Use |
|---|---|---|
| GBDK-2020 4.3.0 (lcc, sdcc 4.4.1, sdasgb, sdldgb, bankpack, makebin, romusage) | GPL / zlib (makebin) — see GBDK `licenses/` | Compiler, assembler, linker, bank packer |
| Python 3 + NumPy | PSF / BSD | Wavetable, graphics, table and track generation; tests |
| SameBoy (optional, tests only) by Lior Halphon | Expat (MIT) | Headless accurate emulator used by `tests/sameboy/harness.c` for timing, audio and save tests |
| RGBDS (optional, to build SameBoy's boot ROMs) | MIT | — |
| PyBoy (optional, tests only) | LGPL v3 | Secondary emulator cross-check |

## Reference studied — GB-303 by furrtek

- <https://github.com/furrtek/GB303>, licence **CC BY 4.0** (as declared in its README: "License: (CC BY 4.0) furrtek 2014~2015").
- Its source was studied to understand acid synthesis on DMG hardware. The files read were `src/play.asm`, `src/playback.asm`, `src/lfo.asm`, `src/tables/drums.asm`, `src/tables/mmap.asm`, `src/tables/wavetables.asm`, `src/tables/bpm.asm`, `src/common/system.asm` and `src/tables/cutofflut.bin`.
- Techniques ETCH shares with GB-303 at the level of ideas:
  - selecting among pre-rendered 16-byte wave-RAM tables to imitate a filter;
  - disabling the CH3 DAC while rewriting wave RAM, then re-triggering;
  - byte-wise lookup tables for distortion;
  - timer-interrupt sequencing;
  - CH1 for kicks and CH4 for hats.
- **No GB-303 code, wavetable data, cutoff tables, drum tables, graphics or text are included in ETCH.**
  - ETCH's wavetables are computed by `tools/generate_waves.py` from an analytic filter model. GB-303's were captured from a TB-303.
  - All ETCH code was written independently for this project.
- Attribution is given in this file, the manual and the README out of courtesy and transparency.

## Trademarks

Roland and TB-303 are trademarks of Roland Corporation. Game Boy is a trademark of Nintendo. ETCH is not affiliated with or endorsed by either company.

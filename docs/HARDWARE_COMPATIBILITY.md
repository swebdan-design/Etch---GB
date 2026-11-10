# ETCH — Hardware Compatibility

**ETCH — A Generative Acid Machine**
by Synth Centric

## Summary

| Item | Value |
|---|---|
| Target | Original Nintendo Game Boy (DMG-01) |
| Also runs on | Game Boy Pocket / Light, Game Boy Color, Game Boy Advance (in DMG mode); accurate emulators |
| Cartridge header | MBC5 + RAM + BATTERY (`$147 = $1B`) |
| ROM | 128 KB (8 × 16 KB banks, `$148 = $02`) |
| Cartridge RAM | 32 KB (4 × 8 KB banks, `$149 = $03`), battery-backed |
| CGB flag | `$00`: a DMG ROM. A Game Boy Color runs it in DMG compatibility mode with its own palette. |
| CPU speed | Normal speed (no CGB double speed is used or required) |
| Extra hardware | None: no custom cartridge, potentiometers, MIDI or link cable |

## Verified so far

**Physical hardware: not yet tested.** No physical Game Boy was available during development. Everything below was verified in emulation only.

- **SameBoy (DMG-B model), the primary reference.** SameBoy has cycle-accurate CPU/PPU/APU timing and models DMG wave-RAM behaviour. The full emulator test suite was run here (`docs/TESTING.md`), along with timing, audio capture and battery saves.
- **SameBoy (CGB-E model).** Boots, displays the title, plays audio, and the timer tick rate matches theory exactly (2571 ticks in 10.04 s).
- **PyBoy 2.9.** Boots, title and LIVE screens render, playback runs and produces audio. PyBoy's timer runs about 2 % slow (250.7 Hz instead of 256 Hz), so it was not used for timing or audio judgements.

## Hardware-sensitive design decisions

- **Wave RAM (CH3).** On the DMG, writing wave RAM or re-triggering CH3 while it is reading can corrupt wave RAM. ETCH always switches the CH3 DAC off (`NR30 = 0`) before writing and re-enables it before triggering. This is the method documented as safe on all models.
- **VRAM access.** Every tile-map and tile write goes through `vram_copy_asm`. It checks the PPU mode and writes each byte with interrupts disabled, so the audio interrupt can never fall between the "VRAM is accessible" check and the write. Interrupts are disabled for at most one mode-2/3 period (under 100 µs). This delays a tick slightly but never drops one.
- **Timing.** All sound and sequencing run from the hardware timer at 256 Hz, never from frame counting. A timer interrupt that arrives during a long tick is latched by the hardware and serviced immediately afterwards. Measured worst-case tick: about 14.6 k of the 16.4 k cycles available.
- **Bank switching in interrupts.** The audio interrupt maps the wavetable and LUT banks and restores GBDK's `_current_bank`. This keeps it safe while banked UI code is running.
- **Input.** The joypad is sampled in the VBlank interrupt and new presses are latched, so short taps are not lost while the main loop is busy computing a mutation (up to about 7 frames, once per phrase).
- **No CGB features.** There is no double speed, no extra WRAM banks, and no CGB palettes. The ROM's behaviour is the same on all models.

## Saving on flash cartridges

ETCH saves to battery-backed cartridge SRAM using the standard MBC5 RAM-enable and bank registers. Whether your saves survive power-off depends on your cartridge:

- **Flash cartridges** (EverDrive GB, EZ-Flash Junior, many cheap repro carts) need SRAM saving supported and enabled. Some write the save back to the SD card only on a specific action or reset. Check your cart's documentation.
- **Repro cartridges** must have a working battery and at least 32 KB of SRAM mapped as MBC5. Carts with only 8 KB SRAM can use slots S1 and S2 only, because those slots live in SRAM bank 0.
- **Emulators** must support MBC5 battery RAM (`.sav` files). Every modern emulator does.

Never-written SRAM (random or `$FF`) is handled safely: slots show `--empty--`. A slot with a valid header but a bad checksum shows `!CORRUPT!` and cannot be loaded.

## Audio output notes

- The DMG mono speaker cuts most energy below about 150 Hz. The bass (C1–B3 fundamentals) and the 64 Hz pulse kick are meant for **headphones or line out**, where the instrument sounds as intended. On the speaker you mainly hear the filter harmonics.
- The SameBoy captures were made with SameBoy's "accurate" high-pass filter, which models the console's output coupling capacitor.
- On Game Boy Color and GBA, CH3's analogue behaviour differs slightly from the DMG (different DAC and filtering). Expect small timbre differences.

## Memory map

| Area | Use |
|---|---|
| ROM bank 0 | GBDK runtime, timer ISR, audio engine, drums, sequencer, UI core, name tables, engine lookup tables (13.9 KB used) |
| ROM bank 1 | Wavetables `[osc 4][res 8][cut 32][16]`: exactly 16 384 bytes |
| ROM banks 2–4 | Transfer LUTs (6 KB), CORROSION, UI screens, graphics, ROM tracks, storage (autobanked) |
| ROM banks 5–7 | Free (about 49 KB) |
| WRAM | Working project 2 133 bytes (73-byte header + 32 patterns × 64 bytes + 12 bytes of drum voicing), undo, candidate and anchor patterns, voiced drum table, 360-byte shadow tile map; 3.4 KB of 4 KB used in WRAM0 |
| SRAM | 8 slots × 4 KB (2 per bank) + a metadata block in bank 3 |

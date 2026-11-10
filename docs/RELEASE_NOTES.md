# ETCH — Release Notes

**ETCH — A Generative Acid Machine**
by Synth Centric

## Version 1.0 (2026-11-10)

First release.

- ROM: `ETCH_by_Synth_Centric.gb`, 128 KB, MBC5+RAM+BATTERY, 32 KB SRAM.
- SHA-256: `8b1bd739c97d7a9d72288d5c07f5f107061e7a08c004ab567eebeacfd0afbf7f`

### What's in it

**Acid bass voice (CH3)**
- 4 oscillators: SAW, SQUARE, HOLLOW, BITE.
- Ladder-filter model: 32 cutoff × 8 resonance pre-rendered tables per oscillator.
- Filter envelope (ENV/DEC), accent envelope with accent charge, VCA envelope.
- Drive: OFF, SOFT, HARD.
- Exponential slide, 4 gate lengths, tune ±12.
- LFO with 4 shapes to pitch, cutoff or resonance.

**Drums**
- 4 lanes × 16 steps, 16 synthesised sounds across CH1, CH2 and CH4.
- Channel priority between lanes.
- Per-lane mute and lock.
- 6 groove presets (ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE).
- Style-aware FILL that resolves at the bar line.

**Sequencer**
- 16 steps: note / rest / tie, accent, slide, probability (1–8/8), 9 playback conditions, step lock.
- 60–220 BPM with exact swing.
- 32 patterns; 16-slot chain with 1–8 bars per slot; song and loop modes.
- Bar-aligned pattern changes; pattern copy and clear.

**CORROSION**
- Seeded grammar-based generation: 6 styles, 6 scales, key, octave, density, syncopation, accent, slide.
- 12-digit reproducible seed codes.
- STABLE / DRIFT / CORRODE evolution with 11 operators.
- Musical memory: anchor recall, motif propagation, homeostasis.
- Step, lane, bass and pattern locks; one-level undo; evolve on demand.

**Interface**
- Screens: LIVE, GRID, DRUM, SYNTH (with a live waveform scope), GEN, CHAIN, SAVE.
- Every function is reachable with D-pad, A, B, START and SELECT.
- Input is latched in VBlank, so short taps are not lost.

**Storage**
- 8 battery-backed slots.
- Versioned headers, Fletcher-16 checksums, verify-after-write.
- Safe handling of empty and corrupt data; last-slot marker.

**Album**
Eight ROM tracks, each a full project with several patterns, a chain, a sound and an evolution mode:
1. FIRST CONTACT
2. CONCRETE
3. STATIC
4. CORROSION
5. NIGHT SHIFT
6. WAREHOUSE
7. SIGNAL LOSS
8. AFTERIMAGE

START at power-on plays track 1.

**Release package**
- ROM, reproducible source archive, 2-minute demo WAV recorded from the ROM.
- Full documentation and test suites: host unit tests and SameBoy integration tests.

### Deviations from the original specification

| Specified | Implemented | Why |
|---|---|---|
| Each pattern "contains synthesis information or references shared settings" | Synth settings are **shared by the whole project** (a song keeps one bass sound) | WRAM budget; one sound per track suits the album format. Use separate save slots for different sounds. |
| A GRID screen with practical drum access | Drums have their own **DRUM** screen, next to GRID | 16 bass steps already fill the GRID screen |
| Probability and conditions | Implemented for **bass steps only**. Drum lanes get variation from FILL and CORROSION instead. | Pattern size (64 bytes) |
| Kick "sub" | The deepest kick bottoms out at **64 Hz**, the lowest frequency of the pulse channels | CH3, which could go lower, is reserved for the bass |
| Smooth analogue filtering | Stepped wavetable selection, with a phase-reset grain during fast sweeps | DMG hardware limits (see `SOUND_ENGINE.md`) |
| GBDK C for audio | C for the logic; **assembly** for the wave-RAM loader, VRAM writes, checksum and 8×8 multiply | SDCC's C was 5–10× too slow inside the interrupt (measured) |

### Known limitations

- **Not yet tested on physical hardware.** Verified in SameBoy (DMG and CGB models) and PyBoy only.
- Undo is one generation deep.
- Seed codes reproduce the initial generated groove only, not edits, sound settings or evolution history. Use save slots for those.
- Project names come from the track they started from and cannot be edited on the device.
- Computing a CORRODE mutation stalls the screen for up to about 7 frames, once per phrase. Audio and button presses are not affected.
- Saves need a cartridge with 32 KB MBC5 SRAM. On 8 KB SRAM carts only slots S1–S2 are usable.
- The Game Boy speaker cannot reproduce the bass; use headphones or line out.

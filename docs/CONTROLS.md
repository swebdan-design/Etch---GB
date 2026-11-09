# ETCH — Controls Reference

**ETCH — A Generative Acid Machine**
by Synth Centric

ETCH uses only the Game Boy's D-pad, A, B, START and SELECT. Three rules cover almost everything:

- **START** always starts or stops the music.
- **SELECT + a button** is a global command. It works on every screen.
- **Hold B + D-pad** changes the value under the cursor (UP/DOWN = fine, LEFT/RIGHT = coarse).

## Global (every screen)

| Buttons | Action |
|---|---|
| START | Play / stop |
| SELECT + RIGHT | Next screen (LIVE → GRID → DRUM → SYNTH → GEN → CHAIN → SAVE → LIVE) |
| SELECT + LEFT | Previous screen |
| SELECT + UP | Queue the next pattern. It switches at the next bar line, and immediately if stopped. This also turns song mode off. |
| SELECT + DOWN | Queue the previous pattern |
| SELECT + A | **Evolve now**: CORROSION mutates the current pattern at the next bar line, or immediately if stopped. In STABLE mode it uses a DRIFT-strength mutation. |
| SELECT + B | **Undo** the last evolution or generation (one level) |

D-pad directions auto-repeat when held: after about 0.3 s, then every 4 frames.

## Title screen

| Buttons | Action |
|---|---|
| START | Enter the instrument. The first track starts playing at once. |

## LIVE — performance

| Buttons | Action |
|---|---|
| UP / DOWN | Select a lane row (BS KK SN CH OH) or a macro row |
| LEFT / RIGHT on a lane | Mute / unmute that lane |
| LEFT / RIGHT on a macro | Adjust by 1. Hold to sweep. |
| B + LEFT / RIGHT on a macro | Coarse adjust (±4; BPM ±10) |
| A | **FILL**: a drum fill until the next bar line. If triggered in the last quarter of the bar, it also covers the following bar. |

Macros: CUT (cutoff), RES (resonance), ENV (envelope depth), DEC (decay), ACC (accent), DRV (drive), BPM, EVO (evolution mode), MUT (mutation intensity).

## GRID — bass steps

Each row is one step. The columns are: playhead, step number, NOTE, A(ccent), S(lide), P(robability), CND (condition), L(ock).

| Buttons | Action |
|---|---|
| UP / DOWN | Select step |
| LEFT / RIGHT | Select field |
| A on NOTE | Cycle REST → NOTE → TIE → REST |
| B + UP / DOWN on NOTE | Semitone up / down. A rest becomes a note. The note auditions when stopped. |
| B + LEFT / RIGHT on NOTE | Octave down / up |
| A on A / S / L | Toggle accent / slide / step lock |
| B + UP / DOWN on A / S / L | Set (UP) or clear (DOWN) |
| B + UP / DOWN on P | Probability 1–8 (8 = always) |
| A on P | Reset to 8 (always) |
| B + UP / DOWN on CND | Cycle condition: ALL 1:2 2:2 1:4 2:4 3:4 4:4 FIL !FL |
| A on CND | Reset to ALL |

## DRUM — percussion

| Buttons | Action |
|---|---|
| D-pad in the grid | Move the cursor (4 lanes × 16 steps) |
| A in the grid | Toggle a hit (auditions when stopped) |
| LEFT / RIGHT on a lane settings row | Choose the KIT, MUTE or LOCK field |
| B + UP / DOWN on KIT | Change the lane's sound (auditions) |
| A on KIT | Audition |
| A on MUTE / LOCK | Toggle |
| B + UP / DOWN on GROOVE | Choose ACID HOUSE / ELECTRO / MINIMAL / BROKEN / INDUSTRIAL / SPARSE |
| A on GROOVE | Apply that groove to all unlocked lanes (undoable) |
| A on GENERATE DRUMS | Generate drums from the GEN settings (undoable) |
| A on CLEAR DRUMS | Clear unlocked lanes (undoable) |

## SYNTH — acid voice

| Buttons | Action |
|---|---|
| UP / DOWN | Select parameter |
| LEFT / RIGHT, or B + UP / DOWN | Adjust by 1 |
| B + LEFT / RIGHT | Coarse adjust (±4; BPM ±10; TUNE ±12) |
| A | Audition (when stopped) |

## GEN — CORROSION

| Buttons | Action |
|---|---|
| UP / DOWN | Select row |
| LEFT / RIGHT, or B + UP / DOWN | Adjust KEY … MODE |
| B + UP / DOWN on SEED | Seed ±1 |
| B + LEFT / RIGHT on SEED | Seed ±256 |
| A on SEED | New random seed, then generate bass and drums |
| LEFT / RIGHT on CODE | Select a digit of the 12-digit seed code |
| B + UP / DOWN on CODE | Change that digit. This edits the parameter it encodes. |
| LEFT / RIGHT on an action row | Choose the left or right action |
| A on an action row | GEN ALL · GEN BASS · EVOLVE · UNDO · LOCK BASS · LOCK PATT |

## CHAIN — song

| Buttons | Action |
|---|---|
| D-pad | Move between SONG, the 16 slots (2 columns), COPY and CLEAR |
| A on SONG | Song mode on / off |
| B + UP / DOWN on a slot | Pattern P01–P32 or END |
| B + LEFT / RIGHT on a slot | Bars 1–8 |
| A on a slot | Song mode: jump there at the next bar. Loop mode: queue that pattern. |
| B + UP / DOWN on COPY | Choose the destination pattern |
| A on COPY | Copy the current pattern to the destination |
| A, A on CLEAR | Clear the current pattern (undoable) |

## SAVE — storage and tracks

| Buttons | Action |
|---|---|
| UP / DOWN | Select slot S1–S8 or track T1–T8 |
| LEFT / RIGHT on a slot | Action: LOAD / SAVE / ERAS(e) |
| A | Arm the action. Press A again to confirm. Saving to an empty slot is immediate. |
| A, A on a track | Load that ROM track (replaces the working project) |

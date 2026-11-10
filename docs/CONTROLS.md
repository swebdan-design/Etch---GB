# ETCH — Controls Reference

**ETCH — A Generative Acid Machine**
by Synth Centric

ETCH uses only the Game Boy's D-pad, A, B, START and SELECT.

## The screen row

The screens sit side by side in one row, with **LIVE** as home:

```
 SYNTH  ◄  TRACK  ◄  LIVE  ►  GEN  ►  SONG  ►  SAVE
```

- **SELECT + LEFT / RIGHT** moves along the row. It stops at the ends; it does not wrap.
- From LIVE, SELECT+LEFT opens the step editor (TRACK), and SELECT+LEFT again opens the sound engine (SYNTH). SELECT+RIGHT takes you back.
- The arrows either side of the screen name in the header show which way you can go.
- Each move briefly shows the neighbouring screens on line 2.

## The editing grammar (every screen)

The same buttons do the same kinds of things everywhere. The idea is borrowed from trackers.

| Buttons | Meaning |
|---|---|
| D-pad | Move the cursor |
| **A** (tap) | Place / toggle / do. On an empty step, A writes something; on a filled step, A changes it. |
| **A held + LEFT / RIGHT** | Value −1 / +1 (semitone for notes) |
| **A held + UP / DOWN** | Value up / down by a big step (octave for notes) |
| **B + A** | Delete, or put back to default |
| **B + D-pad** | Jump: a beat in TRACK, a channel in SYNTH |

On list screens (the LIVE macros, SYNTH, GEN, SONG), plain LEFT/RIGHT also changes the value, so you can sweep a control without holding A.

## Global

| Buttons | Action |
|---|---|
| START | Play / stop |
| SELECT + LEFT / RIGHT | Previous / next screen |
| SELECT + UP / DOWN | Queue the next / previous pattern. It switches at the bar line, and immediately if stopped. Song mode turns off. |
| SELECT + A | **Evolve now**: CORROSION mutates the pattern at the next bar line. In STABLE mode it uses a DRIFT-sized mutation. |
| SELECT + B | **Undo** the last evolution, generation, groove load or clear (one level) |

## LIVE — home: overview, step entry, performance

```
    ▼ .   .   .        playhead
BS ■□■■□□■□■■□■■□□■ ◄  bass        (cursor row marked on the right)
KK ■□□□■□□□■□□□■□□□    kick
SN □□□□■□□□□□□□■□□□    snare
CH □□■□□□■□□□■□□□■□    closed hat
OH □□□□□□□□□□□□□□■□    open hat / perc
CUT ███████      7     macros ...
```

| Where | Buttons | Action |
|---|---|---|
| anywhere | D-pad | Move. LEFT of step 1 is the lane name. |
| lane name | A | Mute / unmute the lane (✕ = muted) |
| bass step | A | Empty step: add a note (the last note you used). Note: tap A to remove it. |
| bass step | A + LEFT / RIGHT | Semitone down / up (the note sounds when stopped) |
| bass step | A + UP / DOWN | Octave up / down |
| bass step | B + A | Remove |
| drum step | A | Hit on / off |
| drum step | B + A | Remove |
| macro row | LEFT / RIGHT | Adjust; hold to sweep |
| macro row | A + UP / DOWN | Adjust coarse (BPM ±10) |
| macro row | A (tap) | **FILL**: a drum fill until the bar line, or through the next bar if late in the bar |

Macros: CUT, RES, ENV, DEC, ACC, DRV, BPM, EVO (mode), MUT (mutation intensity). The bottom line always says what A does where the cursor is.

## TRACK — tracker step editor

One row per 16th step (0–F), with every channel in view:

```
 # NOT ASL KSHO P CD
►0 A-1 A.. K... 8 --
 1 --- ... ..H. 8 --
 2 === ... .... 8 12
```

| Column | Buttons | Action |
|---|---|---|
| NOT | A | Empty step: write the last note |
| NOT | A tap on a note | Turn it into a **tie** (`===`, holds the note above). Tap again: back to a note. |
| NOT | A + LEFT / RIGHT | Semitone down / up |
| NOT | A + UP / DOWN | Octave up / down |
| NOT | B + A | Delete (rest) |
| A S L | A | Accent / slide / step lock on/off. B+A clears. |
| K S H O | A | Kick / snare / closed hat / open hat hit on/off. B+A clears. |
| P | A + D-pad | Chance 1–8 (8 = always). B+A sets 8. |
| CD | A + D-pad | Condition: `--` always, `12` `22` (1st/2nd of every 2 loops), `14` `24` `34` `44` (of every 4), `FI` only in fills, `NF` never in fills. B+A sets `--`. |
| any | B + UP / DOWN | Jump a beat (4 rows) |
| any | B + LEFT / RIGHT | Previous / next pattern |

Moving to a new column shows a one-line hint for it.

## SYNTH — one page per channel

The top row holds the channel tabs: **BAS** (acid bass) · **KCK** · **SNR** · **HAT** · **OPN**.

| Buttons | Action |
|---|---|
| B + LEFT / RIGHT | Previous / next channel (from any row) |
| UP / DOWN | Select a row. Above the first row is the tab row, where LEFT/RIGHT also changes channel. |
| LEFT / RIGHT, or A + LEFT / RIGHT | Adjust by 1 |
| A + UP / DOWN | Adjust coarse |
| B + A | Back to the default value |
| A (tap) | Hear the channel |

**BAS page:** WAVE, CUTOFF, RESO, ENVMOD, DECAY, ACCENT, DRIVE, SLIDE, GATE, LFO RT, LFO DP, LFO TO, LFO SH, TUNE. A live scope shows the waveform being played.

**Drum pages:**

| Row | Meaning |
|---|---|
| SOUND | The lane's sound in the current pattern (P01 …) |
| TUNE | Pitch, −8 to +7 |
| DECAY | Length, 0–15 (8 = as designed) |
| LEVEL | Volume, 0–15 |
| MUTE | A toggles |
| LOCK | A toggles; protects the lane in this pattern from CORROSION |

## GEN — CORROSION

| Row | Buttons |
|---|---|
| KEY … MODE | LEFT/RIGHT or A+D-pad adjusts |
| SEED | A+LEFT/RIGHT ±1, A+UP/DOWN ±256. **A tap: new random seed + generate.** B+A: reset the seed. |
| CODE | LEFT/RIGHT picks a digit, A+D-pad changes it |
| actions | LEFT/RIGHT picks, A runs: **GEN ALL · GEN BASS**, **GEN DRUMS · GROOVE** (loads the STYLE's drum groove), **EVOLVE · UNDO**, **LOCK BASS · LOCK PATT** |

## SONG — tempo and arrangement

| Row | Buttons |
|---|---|
| SONG MODE | A toggles: ON = follow the chain, OFF = loop the current pattern |
| BPM | LEFT/RIGHT ±1, A+UP/DOWN ±10 |
| SWING | LEFT/RIGHT, 0–8 |
| chain slot | LEFT/RIGHT switches column. A+LEFT/RIGHT sets the pattern (or END). A+UP/DOWN sets bars 1–8. **A tap jumps there at the next bar** (in loop mode, queues the pattern). B+A sets END. |
| COPY | A+D-pad chooses the destination; A copies the current pattern |
| CLEAR | A, A clears the current pattern (undoable) |

## SAVE — slots and the album

| Buttons | Action |
|---|---|
| UP / DOWN | Slot S1–S8 or track T1–T8 |
| LEFT / RIGHT | Slot action: LOAD / SAVE / ERAS |
| A | Arm. Press A again to confirm. Saving to an empty slot is immediate. |
| A, A on a track | Load that ROM track |

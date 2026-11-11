# ETCH — CORROSION, the Generative System

**ETCH — A Generative Acid Machine**
by Synth Centric

> A machine that makes acid music. A machine that slowly rewrites itself.

CORROSION is ETCH's generative engine. It does two things:

1. It **composes** a complete bass line and drum groove from a seed and a small set of musical parameters.
2. It **evolves** a playing pattern at phrase boundaries, either restrained (DRIFT) or aggressively (CORRODE). Every lock is respected, and the material keeps a recognisable identity.

All of CORROSION is portable C (`src/generator/corrosion.c`, `pattern.c`). It is compiled both for the Game Boy and natively for the host unit tests in `tests/test_generator.c`. It uses only small loops, lookups and a 16-bit xorshift PRNG; there is no search or backtracking. A typical mutation takes a few frames of main-loop time on the DMG. The audio interrupt never waits for it.

## 1. Parameters (GEN screen)

| Parameter | Range | Musical meaning |
|---|---|---|
| KEY | C–B | Tonal centre (root) |
| SCALE | MINOR, MAJOR, DORIAN, PHRYGIAN, MIN PENT, CHROMATIC | Allowed pitch classes |
| STYLE | ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE | Rhythmic grammar (below) |
| OCTAVE | 1–3 | Register of the root. The line lives from a fourth below the root to two octaves above it. |
| DENSITY | 0–15 | How many steps sound; also hat and drum variation density |
| SYNCOP | 0–15 | Pushes notes off the beat (onto "e" and "a" 16ths) and away from beats 2–4 |
| ACCENT | 0–15 | Accent probability; also the accent target that evolution orbits |
| SLIDE | 0–15 | Slide probability (scaled by the style's slide habit) |
| MUTATE | 0–15 | Mutation budget per evolution |
| MODE | STABLE, DRIFT, CORRODE | Evolution behaviour |

The GEN screen also has the actions GEN ALL, GEN BASS, GEN DRUMS, GROOVE (loads the STYLE's drum template), EVOLVE, UNDO, LOCK BASS and LOCK PATT.
| SEED | 0000–FFFF | Random seed for generation |

## 2. Composition (GENERATE)

Generation is **grammar driven**. Each style defines:

- **Step weights** (16 values): how likely each 16th is to carry a note. For example, ACID favours busy 16ths, ELECTRO the off-grid positions 3, 6, 10, 11 and 14, MINIMAL the off-beats.
- **Accent weights** per step. Acid lines accent off-beats; INDUSTRIAL accents the beat.
- **Motif lengths**, e.g. ACID {4, 8}, BROKEN {3, 5, 6}, SPARSE {8, 16}. Odd lengths in BROKEN make the motif drift against the four-beat bar (polymeter).
- **Habits:** kick avoidance, octave-jump rate, slide bias, tie rate, ghost-note rate, phrase-answer rate, base vocabulary size.
- **A drum template plus variation positions** for each lane, and a preferred kit.

Algorithm, executed in this order, with every random draw coming from the seeded PRNG:

1. **Drums:** template hits, plus variation hits chosen with probability DENSITY/2 (hats on off-16ths also respond to SYNCOP). Very low density thins the hats.
2. **Vocabulary:** the root, then intervals in order of tonal importance (octave, fifth, minor third, minor seventh, fourth, second, sixth, a seventh below, minor second). Each is snapped to the scale, until the style's vocabulary size is reached (2–7 notes; some candidates are skipped at random so seeds differ). A line is built from a few pitches, not sixteen unrelated ones.
3. **Motif:** for each motif step, a note sounds with probability `weight × (DENSITY+4)/14`, adjusted by SYNCOP. Strong positions prefer the root. Other positions prefer the first, most tonal, vocabulary entries. Occasional octave jumps follow the style's habit. Accents follow `accent weight × ACCENT`. The downbeat always carries the root (the tonal centre) unless density is very low.
4. **Repetition:** the motif is tiled across 16 steps.
5. **Phrase answer:** with the style's answer probability, beat 4 answers the motif. The answer either transposes up a third/fourth in the scale, swaps a note for a rest and adds an accented pickup, or lifts the last notes an octave. This gives the classic A-A-A-B shape.
6. **Kick relationship:** bass notes that coincide with a kick (except on the downbeat) are dropped with the style's kick-avoidance probability. ELECTRO and SPARSE leave room for the kick; INDUSTRIAL doubles it.
7. **Articulation:** slides are placed between consecutive notes (probability `SLIDE × slide bias`). Rests after a note may become ties (SPARSE, MINIMAL). Off-beat notes may become ghost notes with 5/8 or 6/8 probability (BROKEN, SPARSE).
8. If ACCENT > 5 and no accent was placed, the highest-weighted note gets one.
9. `pattern_sanitize` makes the result valid. Slides must lead into a note, ties must follow sound, and ranges are clamped.

**Locks during GENERATE:**

- Locked steps, locked drum lanes, a locked bass line and a locked pattern are kept.
- The full random stream is consumed regardless of locks, so the unlocked material is identical to what the seed produces in an unlocked pattern.
- GENERATE saves the previous pattern for UNDO.

### Seed codes

The CODE row shows a 12-digit hex code:

```
SSSS-KCYD-PALO   seed(4) Key sCale stYle Density syncoP Accent sLide Octave
```

Entering the same code (digit by digit on the CODE row) and pressing GEN ALL on an empty, unlocked pattern **reproduces exactly the same initial groove**. Host test 10 verifies this.

**Limitation:** a seed code describes only the initial generated groove. It does **not** capture manual edits, kit changes, locks, synth settings or evolution history. To keep those, save the project to a SAVE slot.

## 3. Evolution (MODE)

The main loop prepares the next mutation in the background (`evo_service` in `src/main.c`), for the pattern that will play after the next bar line. In song mode the chain usually changes pattern exactly on a phrase boundary, so targeting the next pattern matters. It works on a working copy, so pattern memory is never half-modified. The timer interrupt **commits** the candidate at the gate point of step 16, so the new material starts exactly on the next downbeat. That moment is a tick with no step work, which keeps the audio timing stable.

When a candidate is committed:

- The previous pattern is copied to the undo buffer first, so `SELECT+B` always restores the previous generation.
- If you edit the pattern by hand, the candidate is discarded and recomputed from your edit.
- If playback moves to another pattern, the candidate is retargeted.

| Mode | When | Budget per evolution | Character |
|---|---|---|---|
| **STABLE** | never (except on request) | – | Pattern memory is never rewritten. Probability and conditions still vary playback. |
| **DRIFT** ("living song") | every 4 bars (one phrase) | **Every channel:** 1 bass operation (2 at MUTATE ≥ 12), one hit in each unlocked drum lane, and a sound-drift step | Subtle, constant variation **around an anchor**, different on every listen |
| **CORRODE** | every 2 bars | 2 + MUTATE/3 operations (2–7) | The line keeps evolving into new material descended from the original |
| *request* (SELECT+A) | next bar line | as the current mode; STABLE uses DRIFT | Manual |

### Mutation operators

| Operator | Effect | DRIFT weight | CORRODE weight |
|---|---|---|---|
| ACC_MOVE | Move, add or remove an accent, steered toward the ACCENT target | 3 | 2 |
| REST_TOGGLE | Note ↔ rest (never the downbeat), steered toward the DENSITY target | 2 | 2 |
| NOTE_STEP | Move a note one scale step. DRIFT never touches the downbeat; CORRODE only snaps it into the scale | 2 | 2 |
| OCTAVE | Octave up or down within range | 2 | 2 |
| SLIDE | Toggle a slide between two notes | 2 | 2 |
| DRUM | Toggle a hit, preferring the style's variation positions. DRIFT never removes the template kick or backbeat | 3 | 3 |
| MEMORY | Copy beat 1 onto the most divergent beat (restores repetition) | 1 | 3 |
| MOTIF | Change one step of the beat-1 motif **and every repetition of it** (identity evolves, repetition is kept) | 1 | 5 |
| ROTATE | Rotate one beat by a 16th (syncopation shift) | 0 | 1 |
| PROB | Make a note a ghost note, or a ghost note solid again | 0 | 1 |
| RECALL | Restore one differing step or hit from the anchor | 1 | 0 |

### Musical memory

ETCH keeps evolution coherent with these rules:

- **Anchors, per pattern (DRIFT):** the anchor is the pattern as it was when DRIFT first touched it, or when you last edited it by hand. Up to 8 patterns keep their own anchor (a 512-byte cache), so every pattern of a song breathes around its own original.
- **Per-channel bounds (DRIFT):** the bass may differ from its anchor by at most `2 + MUTATE/6` steps. Each drum lane may differ by at most `1 + (MUTATE+4)/8` hits. When a channel reaches its bound, that channel's change for the phrase becomes a recall toward the anchor. Every channel still moves every phrase, but nothing wanders off.
- **Single-step operators only (DRIFT):** accent move, rest toggle, scale step, octave, slide. The multi-step operators (MOTIF, MEMORY, ROTATE) belong to CORRODE.
- **Sound drift:** after each committed evolution, the bass cutoff takes a random step within ±2 (CORRODE ±5), and each drum lane's tuning within ±1 (CORRODE ±2). These are live offsets in the engine (`evo_cut_ofs`, `evo_tune_ofs`). They are never stored in the project, and they reset to zero in STABLE.
- **Randomness:** evolution draws from an entropy pool stirred by the exact timing (DIV, TIMA, frame counter) of every button press, including the START press on the title screen. The probability dice are reseeded the same way at every START. Two listeners, or two listens, therefore evolve differently in practice. Generation from a seed code stays fully reproducible, because it uses the seed, not the pool.
  - *Limits:* the main loop is locked to the 60 Hz frame, so the effective entropy is mostly the frame on which START was pressed: roughly a thousand distinct starts for human press times of 2–20 s. The pool is 16 bits. Scripted or replayed input reproduces the same evolution. See the manual, §12 *Will every listen be unique?*
- **Downbeat and tonal centre:** step 1 keeps its pitch. DRIFT never rests it and never removes template kicks or backbeats.
- **Motif propagation (CORRODE):** changing the motif changes all its repetitions together, so the line can transform completely while staying internally repetitive.
- **Homeostasis:** DENSITY and ACCENT define targets that the rest and accent operators steer toward. This prevents slow drift into "every step an accented note" or into silence.

**Measured** (host tests; 30 grooves × 16 generations, intensity 8):

| | Channels changed per phrase | Divergence from the original | Bass steps changed | Best-period repetition |
|---|---|---|---|---|
| DRIFT | **all 5 in 480 / 480 phrases** | **7.4 events** (max 12, bounded) | 2.4 | 0.46 |
| CORRODE | – | **24.9 events** | 14.0 | 0.60 |

DRIFT's repetition (0.46) is lower than a fresh line's (0.88) because a few steps of each bar are always varied. That is the "subtle change every 4 bars" at work. Locked lanes are never touched (host test 8b).

Freshly generated lines have an average repetition of 0.88. An example CORRODE lineage from the test output (A minor, ACID):

```
g0  A2 .  E3 .  | .  .  E3 =  | .  .  E3 =  | A2s A3! .  .
g4  A2 D3 E3! . | A2 D3 E3! . | .  E2 E3! = | A2! D3 E3! .
g8  A2 C3 E2! . | A2 .  E3! . | A2 C3 E2! . | .  C3s E3! .
g12 A2 C3 E2  . | A2 C3 E2  . | A2 C3 E2  . | A2 C3 E2  .
```

The riff transforms (A–E → A–D–E → A–C–E, with accents appearing and moving, then settling into a hypnotic A–C–E loop) without losing its pulse or key.

### Locks and undo guarantees

- **Step lock (TRACK, L column), lane lock (SYNTH drum page, LOCK), bass lock and pattern lock (GEN):** after every mutation, all locked material is copied back from the source bit-for-bit. Host tests ran 8 000 mutations with step and lane locks set: zero violations, zero invalid patterns. The emulator test checks a locked step and the locked kick lane on the real ROM.
- **Undo:** one generation deep. The undo buffer is written only at the moment a mutation (or GENERATE, groove, clear) actually commits.

## 4. Fills

FILL (A on LIVE) derives temporary drum masks from the current groove and style. Examples: an ACID 16th-snare roll into the next bar with an open-hat lift; INDUSTRIAL machine-gun snare and 16th hats; ELECTRO snare flams.

The fill replaces the drum masks only until the next bar line, or the bar after if triggered in the last quarter. The pattern itself is not modified. Steps with condition FIL / !FL respond to fills.

## 5. Probability and conditions (playback variation)

These work in every mode, including STABLE:

- **Probability** p/8 is rolled live each time the step comes round. It uses a PRNG separate from the composition PRNG, so live randomness never affects seed reproducibility.
- **Conditions:**
  - `1:2`, `2:2`: play on the first or second of every two loops of the pattern.
  - `1:4` … `4:4`: play on that loop of every four.
  - `FIL`: play only during a fill. `!FL`: play only outside one.

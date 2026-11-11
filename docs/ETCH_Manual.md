# ETCH — A Generative Acid Machine
### by Synth Centric

**User Manual — version 2.5**

![ETCH title screen](images/screen_title.png)

---

## 1. Introduction

ETCH turns a Game Boy into an acid bass synthesizer, a four-lane drum machine, a 16-step sequencer and a generative composer.

It is also a **Synth Centric music release**. The cartridge holds eight demo songs. Each one is a starting point that you can play as written, reshape, mutate and save as your own version. The instrument itself is the artwork.

The demo songs are **living songs**: they start in DRIFT mode, so every four bars every channel changes a little. The changes are driven by the moment you press START and the timing of your button presses, so in practice every listen is different (see *Will every listen be unique?* in §12).

**Who made it.** Dan Schwebel, known as Synth Centric, is an Australian beat maker, music producer, writer and educator. ETCH is a Synth Centric release.

> *A machine that makes acid music. A machine that slowly rewrites itself.*

ETCH draws on the Roland TB-303 and on furrtek's GB-303 project, but its sound engine, sequencer, generative system and interface are original.

---

## 2. Hardware compatibility

- Original Game Boy (DMG), Game Boy Pocket and Light, Game Boy Color (in DMG mode), and Game Boy Advance / SP (in DMG mode).
- Any accurate emulator (SameBoy, BGB, Gambatte, mGBA, Emulicious and others).
- Cartridge type: MBC5 with 32 KB battery-backed RAM, 128 KB ROM.
- **Use headphones or line out.** The Game Boy speaker cannot reproduce bass.

ETCH has so far been verified in emulators only. See `HARDWARE_COMPATIBILITY.md` for details and flash-cartridge saving notes.

---

## 3. Installation and ROM use

- **Emulator:** open `ETCH_by_Synth_Centric.gb`. Saves go to the emulator's battery file (usually `ETCH_by_Synth_Centric.sav` next to the ROM).
- **Flash cartridge:** copy the `.gb` file to the SD card and make sure SRAM saving is enabled for it.
- **Building from source:** see `README.md`.

---

## 4. Quick start

1. Switch on. The ETCH title appears.
2. Press **START**. Demo song 1, *CORRODE*, starts immediately, and you are on **LIVE**, the home screen.
3. Press **DOWN** five times to reach **CUT**, then hold **RIGHT**. The filter opens and the bass starts to scream. Hold **LEFT** to close it again.
4. Move up into the lanes. On the **KK** row, move to an empty step and press **A**: you have added a kick. Press **A** again to take it away.
5. On the **BS** row, press **A** on an empty step to add a bass note. Hold **A** and press **RIGHT** to raise it a semitone, or **UP** for an octave.
6. Tap **B** for a drum fill; tap **B** again to drop back into the groove.
7. Move down to **RGN** and tap **A**: half the bar is instantly replaced with something new. Press **SELECT + B** to undo it.
8. Press **SELECT + A**. At the next bar line, CORROSION rewrites a little of the line. **SELECT + B** undoes this too.
9. Press **SELECT + LEFT** to open the step editor, and **SELECT + LEFT** again for the sound engine. **SELECT + RIGHT** brings you back.
10. Press **START** to stop.

---

## 5. Controls

### The screen row

```
 SYNTH  ◄  TRACK  ◄  LIVE  ►  GEN  ►  SONG  ►  FILE
```

**LIVE** is home. **SELECT + LEFT/RIGHT** moves along the row and stops at either end. Moving left takes you deeper into the music: first the step editor (TRACK), then the sound of each channel (SYNTH). Moving right takes you to the tools: the composer (GEN), the arrangement and tempo (SONG), and your files (FILE: new, load, save, about). Each move briefly shows the neighbouring screens on line 2.

### One way of editing everywhere

ETCH's editing borrows the habits of trackers:

| | |
|---|---|
| **D-pad** | Move the cursor |
| **A** | Put something there or switch it. On an empty step, A writes a note or hit; on a filled one, A changes it. |
| **Hold A + LEFT/RIGHT** | Change a value by 1 (a semitone for notes) |
| **Hold A + UP/DOWN** | Change a value by a big step (an octave for notes) |
| **B + A** | Delete, or reset to default |
| **B + D-pad** | Jump: a beat in TRACK, a channel in SYNTH |

On the macro and parameter lists, plain LEFT/RIGHT also changes the value, so you can sweep a sound with one thumb.

### The header

```
◄LIVE ►P01S03 124xU ▶
```

| Element | Meaning |
|---|---|
| ◄ ► | Screens exist to the left / right |
| `P01` | The pattern now playing |
| `>02` | The pattern queued for the next bar (shown instead of the S field) |
| `S03` | Song position, when song mode is on |
| Fill mark (left of the BPM) | A one-shot fill is playing; an inverted **F** means the fill is latched on |
| `124` | BPM |
| `x` over a bar | Evolution (DRIFT or CORRODE) is on |
| `U` | An undo is available |
| ▶ / ■ | Playing (blinks on the beat) / stopped |

Line 2 shows messages such as SAVED, EVOLVED and UNDO, the screen map, and column hints.

### Global controls

| | |
|---|---|
| START | Play / stop |
| SELECT + LEFT / RIGHT | Move along the screen row |
| SELECT + UP / DOWN | Queue the next / previous pattern (switches at the bar line) |
| SELECT + A | Evolve now |
| SELECT + B | Undo |
| SELECT + START | FILL on / off (latched) |

The complete list is in `CONTROLS.md`.

---

## 6. LIVE — home, step entry and performance

![LIVE](images/screen_live.png)

The top half is the whole pattern at a glance, and you can edit it directly:

- The **BS** row is the bass line. A filled cell is a note, a framed cell an accent, a cell with a tail a slide, and a bar a tie.
- **KK SN CH OH** are kick, snare, closed hat and open hat/percussion.
- The ▼ playhead walks along the top. The ◄ on the right marks the row you're on.
- The small column after each lane name flashes when the lane plays, and shows ✕ when it is muted.

**Step entry on LIVE:**

| Where | What to press |
|---|---|
| Drum step | **A** puts a hit there or takes it away |
| Bass step | **A** on an empty step adds a note: the last note you used. **Tap A** on a note to remove it. **Hold A + LEFT/RIGHT** moves it a semitone; **A + UP/DOWN** an octave. When stopped, you hear each note as you set it. |
| Lane name (left of step 1) | **A** mutes or unmutes the lane |
| Any step | **B + A** clears it |

Accents, slides, ties, probability and conditions are set in TRACK.

**FILL:** tap **B** anywhere on LIVE (or SELECT + START on any screen) to switch the drum fill on; it keeps playing bar after bar until you tap again, when the groove comes straight back. For a single fill that ends by itself at the bar line, tap **A** on a macro row (except RGN).

The lower half holds nine **macros**. Move to one with UP/DOWN. **LEFT/RIGHT** changes it (hold to sweep), and **A + UP/DOWN** jumps in big steps. The bottom line always explains the row you're on.

| Macro | Range | What it does | What you hear / when to use it |
|---|---|---|---|
| **CUT** | 0–31 | Filter cutoff: the basic brightness of the bass | Low = dark, muffled, underwater. High = bright, buzzing, aggressive. This is *the* acid control: slow sweeps of CUT are the sound of acid house. |
| **RES** | 0–7 | Resonance: a peak at the cutoff | Low = smooth. High = the squelchy, "talking", whistling sound. Turn it up when you want the line to scream; it is most dramatic while CUT or ENV is moving. |
| **ENV** | 0–15 | Envelope amount: how far each note opens the filter on top of CUT | 0 = every note has the same tone. High = every note "wows" open and closes again. Pairs with DEC. |
| **DEC** | 0–15 | Decay: how quickly that envelope closes | Low = short, plucky, percussive notes. High = long, evolving sweeps. Raising DEC while RES is high gives the classic long squelch. |
| **ACC** | 0–15 | Accent strength | How much louder and sharper the accented notes are (A column in TRACK). Higher ACC = a bigger difference between accented and plain notes. Runs of accents build up and get brighter. |
| **DRV** | OFF / SOFT / HARD | Distortion | SOFT = warm, thicker saturation. HARD = buzzy digital clipping. Changes the attitude rather than the volume. |
| **RGN** | ALL / BASS / DRUMS | **REGEN**, an action | **Tap A**: exactly half of the bar (8 of the 16 steps, picked at random) is instantly replaced with new material in the song's key, scale and style. LEFT/RIGHT chooses what is replaced: ALL (bass and drums at those 8 steps), BASS only or DRUMS only. Locked steps and lanes are never touched. **SELECT + B** undoes it. |
| **EVO** | STABLE / DRIFT / CORRODE | Evolution: how the song changes by itself | STABLE = never changes. DRIFT = a *living song*: every 4 bars every channel changes a little, then drifts back toward the original. CORRODE = bigger changes every 2 bars; the song slowly turns into something new. See §12. |
| **MUT** | 0–15 | Mutation: how much evolution changes each time | Only matters with EVO on DRIFT or CORRODE. Low = barely noticeable variations. High = more changes per phrase, and further from the original before DRIFT pulls back. Around 4–6 is subtle; 10+ is restless. |

**Tempo (BPM) is on the SONG screen** (two screens to the right), together with swing. It also still shows in the header.

### Ways to change the music on the fly

| | How | What changes | How long it lasts |
|---|---|---|---|
| **FILL (latched)** | **Tap B** on LIVE, or **SELECT + START** on any screen | Drums only: the song's drum fill | **On until you switch it off** (same button). Off returns to the groove immediately. |
| **FILL (one-shot)** | Tap A on any macro row except RGN | The same fill, once | Until the end of the bar (or the next bar if late), then the groove returns by itself |
| **REGEN** | Tap A on the RGN row | Exactly 8 of the 16 steps get new material: bass, drums or both | Permanent (undo with SELECT + B) |
| **EVOLVE NOW** | SELECT + A, anywhere | A small mutation, as in DRIFT (or CORRODE if that is the mode) | Permanent, starting at the next bar (undo with SELECT + B) |
| **EVO** | Set the EVO row to DRIFT or CORRODE | The song keeps changing by itself, every 4 or 2 bars | Until you set STABLE |

REGEN's 8 steps are exactly half the bar. If tidying is needed, a neighbouring slide marker that now leads into silence, or a tie that now follows silence, is removed. These markers had no audible effect any more, so you hear exactly 8 steps change.

Sound changes are immediate. Pattern changes wait for the bar line, and evolutions for the end of a phrase, so nothing knocks the music out of time.

---

## 7. Programming steps — the TRACK screen

![TRACK](images/screen_track.png)

TRACK is a tracker page: one row per 16th step, numbered 0–F, with every channel side by side.

```
 # NOT ASL KSHO P CD
►0 A-1 A.. K... 8 --
 1 --- ... ..H. 8 --
 2 === ... .... 8 12
```

| Column | Meaning |
|---|---|
| NOT | `A-1` is a note, `---` a **rest** (the note before is released), `===` a **tie** (the note before keeps sounding) |
| A S L | **A**ccent (louder, sharper filter snap); **S**lide (hold the note and glide into the next one, the acid "bend"); **L**ock (CORROSION never changes this step) |
| K S H O | Kick, snare, closed-hat and open-hat hits |
| P | **Probability** 1–8. 8 always plays; 4 plays half the time, rolled again every loop. |
| CD | **Condition**. `--` always. `12`/`22`: only on the 1st/2nd of every two loops. `14`–`44`: one loop of every four. `FI`: only during fills. `NF`: never during fills. |

Editing:

- Move with the D-pad. **B + UP/DOWN** jumps a beat.
- **NOT:** **A** on an empty step writes the last note you used. **A + LEFT/RIGHT** changes it by a semitone, **A + UP/DOWN** by an octave. **Tap A** on a note to make it a tie; tap again to make it a note. **B + A** deletes.
- **A S L** and **K S H O:** **A** switches them on or off.
- **P** and **CD:** hold **A** and use the D-pad. **B + A** puts them back to "always".
- **B + LEFT/RIGHT** moves to the previous or next pattern.

When you move to a new column, line 2 briefly reminds you what A does there.

> **Workflow: a classic acid line from scratch**
> 1. On SONG, CLEAR the pattern (A, A), or pick an empty pattern with SELECT+UP.
> 2. In TRACK, on rows 0, 3, 6, 8, B and E press A to write A-1. Make rows 2 and A an octave higher (A + UP). Make row 5 C-2 and row D E-2.
> 3. Accent rows 3, 8 and E (A column). Add slides on 5 and A (S column).
> 4. In the K column put hits on 0, 4, 8 and C. Put H hits on 2, 6, A and E.
> 5. SELECT+RIGHT to LIVE: CUT about 8, RES up, ENV 10, DEC 6. Press START and ride CUT and DEC.

---

## 8. Drums

Drums live in three places:

- **LIVE**, for quick hits while you play.
- **TRACK** (K S H O columns), for precise programming.
- **SYNTH** (KCK, SNR, HAT, OPN pages), for the sound.

| Lane | Sounds (SOUND row) |
|---|---|
| KK (kick) | DEEP, PUNCH, THUD, CLICK |
| SN (snare) | SHORT, GRIT, CLAP, RIM |
| CH (closed hat) | CLOSED, TICK (metallic), SHAKE, SHARP |
| OH (open hat / perc) | OPEN, METAL, BLIP (tonal), CRASH |

Each drum page also has **TUNE**, **DECAY** and **LEVEL** for the lane, plus **MUTE** and **LOCK**.

Whole grooves come from GEN:

- **GROOVE** loads the drum pattern of the selected STYLE: ACID HOUSE (four-on-the-floor, clap backbeat, off-beat hats), ELECTRO (syncopated 808-style kick), MINIMAL, BROKEN (displaced kick), INDUSTRIAL (relentless 16ths) or SPARSE (half-time).
- **GEN DRUMS** writes a new groove from the GEN settings.

Both can be undone with SELECT + B.

The Game Boy has three channels for drums, so snare, closed hat and open hat share the noise channel. When they land on the same step, the snare wins over the open hat, which wins over the closed hat.

---

## 9. The sound engine — the SYNTH screen

![SYNTH](images/screen_synth.png)

SYNTH has one page per channel. The tabs along the top are **BAS** (acid bass), **KCK** (kick), **SNR** (snare), **HAT** (closed hat) and **OPN** (open hat / percussion). **B + LEFT/RIGHT** flips pages from anywhere. On every page, **LEFT/RIGHT** (or **A + D-pad**) changes the selected value, **B + A** puts it back to default, and **tapping A** lets you hear the channel.

**BAS page.** The **SCOPE** shows the actual waveform the Game Boy is playing at this moment; watch it change as the filter moves.

| Parameter | Sound |
|---|---|
| **WAVE** | SAW is the classic, raw acid tone. SQR is hollower and more nasal. HOLW is round and soft, so the resonance sings. BITE is a synced, metallic digital tone. |
| **CUTOFF / RESO** | Brightness (0–31) / squelch (0–7) |
| **ENVMOD / DECAY** | How far, and for how long, each note opens the filter. DECAY also slows the volume decay. |
| **ACCENT** | Accent strength. Consecutive accents build up and get brighter. |
| **DRIVE** | OFF / SOFT / HARD distortion |
| **SLIDE** | Glide time, from instant to a slow, lazy bend |
| **GATE** | Note length: SHRT, MED, LONG, FULL |
| **LFO RT / DP / TO / SH** | Low-frequency oscillator. RT is rate (0.1–22 Hz) and DP is depth. TO is the target: PTCH (vibrato), CUT (auto-wah), RES (resonance wobble). SH is the shape: TRI, SQR, SAW or S&H (random steps). |
| **TUNE** | Transposes the bass ±12 semitones |

**Drum pages:**

![SYNTH, kick page](images/screen_synth_kick.png)

| Row | Sound |
|---|---|
| SOUND | Which sound this lane uses in the current pattern (shown as `(P01)`). Each pattern can have its own kit. |
| TUNE | Higher or lower pitch: a deeper kick, a tighter snare, a brighter hat |
| DECAY | Shorter or longer ring |
| LEVEL | Volume of the lane |
| MUTE | Silence the lane |
| LOCK | Protect the lane in this pattern from CORROSION |

TUNE, DECAY and LEVEL apply to the whole project, so every pattern shares them.

How it works (in brief): the Game Boy has no filter. ETCH holds 1 024 pre-calculated waveforms, one for each oscillator, resonance and cutoff combination, rendered from a model of a resonant 4-pole filter. It switches between them many times a second as the envelopes move. See `SOUND_ENGINE.md`.

---

## 10. Using CORROSION — the GEN screen

![GEN](images/screen_gen.png)

CORROSION is ETCH's composer. It doesn't throw random notes around. It works with a **key and scale**, a small **pitch vocabulary**, a **motif** that repeats, **accent and slide habits**, and a **relationship with the kick drum**, all shaped by a **style**.

These settings shape everything CORROSION writes: GENERATE, GROOVE, REGEN, and the notes evolution chooses.

| Setting | Range | What it does |
|---|---|---|
| **KEY** | C–B | The home note of the song. Every generated line starts and resolves here. |
| **SCALE** | MINOR, MAJOR, DORIAN, PHRYGIAN, MIN PENT, CHROMATIC | Which notes are allowed. MINOR is classic dark acid. DORIAN is dark but hopeful. PHRYGIAN is the most sinister (flat second). MIN PENT is safe and bluesy, with no wrong notes. CHROMATIC is anything goes. |
| **STYLE** | ACID HOUSE, ELECTRO, MINIMAL, BROKEN, INDUSTRIAL, SPARSE | The rhythmic personality: where notes and drum hits like to fall, how long the repeating motif is, and how the bass works around the kick (see §8). Also picks the groove that GROOVE loads. |
| **OCTAVE** | 1–3 | How low or high the bass line sits |
| **DENSITY** | 0–15 | How busy: more notes and more drum variations as it rises |
| **SYNCOP** | 0–15 | Syncopation: how much the line avoids the beat and lands on the off-beats instead. High = funkier, more broken. |
| **ACCENT** | 0–15 | How many accents the generator writes. Evolution also keeps roughly this many. |
| **SLIDE** | 0–15 | How many slides (glides between notes) the generator writes |
| **MUTATE** | 0–15 | The same setting as MUT on LIVE: how much each evolution changes |
| **MODE** | STABLE, DRIFT, CORRODE | The same setting as EVO on LIVE |
| **SEED** | 0000–FFFF | Which version of a groove you get from these settings. Same seed + same settings = same groove. |
| **CODE** | 12 digits | The seed and settings in one shareable code (§11) |

The action rows:

- **GEN ALL** writes a complete new bass line and drum groove.
- **GEN BASS** writes only the bass line.
- **GEN DRUMS** writes only the drums; **GROOVE** loads the STYLE's drum groove.
- **EVOLVE · UNDO** and **LOCK BASS · LOCK PATT** are also here.
- **Tap A on the SEED row** to pick a new random seed and generate in one press. Keep pressing it to audition ideas.

> **Workflow: generate a complete groove**
> 1. GEN: KEY A, SCALE MINOR, STYLE ACID HOUSE, DENSITY 9, ACCENT 8, SLIDE 7.
> 2. Highlight SEED and tap **A** until you hear something you like.
> 3. Go to LIVE and shape it with CUT, RES and ENV.
> 4. Note the **CODE** so you can find it again.

---

## 11. Musical generation and random seeds

The **CODE** row shows a 12-digit code such as `0F1C-9009-5870`. It packs the seed, key, scale, style, density, syncopation, accent, slide and octave.

To recreate a groove, select the CODE row, step through the digits with LEFT/RIGHT, set each with **A + UP/DOWN**, then press **GEN ALL** on an unlocked pattern. You get exactly the same initial groove, every time, on every Game Boy.

**What a code does not store:** manual edits, kit changes, sound settings, locks and evolution history. Use FILE > SAVE to keep those.

---

## 12. Locks, evolution and undo

### Evolution modes (EVO on LIVE, MODE on GEN)

| Mode | Behaviour |
|---|---|
| **STABLE** | The pattern stays exactly as written. Probability and conditions still add variation as it plays. |
| **DRIFT** | **A living song.** Every 4 bars, **every channel changes a little**: <ul><li>One small bass change: a note steps within the scale, an accent moves, a slide appears…</li><li>One hit in each drum lane.</li><li>A small step in the sound: bass filter, drum tuning.</li></ul> DRIFT **remembers your groove**, separately for every pattern of the song. Each channel can only stray a short way from the original before its changes start leading back. The randomness comes from the timing of your button presses, so in practice every listen is different (see below). All the demo songs start in DRIFT. |
| **CORRODE** | Every 2 bars, bigger changes. The motif itself mutates, and its repetitions change with it, so over a few minutes the line becomes something new that is clearly descended from where it started. |

**MUT** (0–15) sets how far things may wander: with higher MUT the bass makes more changes and every lane may stray further.

The sound drift (filter and drum tuning) is applied live and is **never written into your settings**: switch EVO to STABLE and the sound returns to exactly what you set.

> **Workflow: a different song every listen**
> Load a demo (FILE > LOAD, or just press START at power-on). On LIVE, put the cursor on **EVO**: the bottom line reads `EVOLVES EVERY 4 BARS`. Leave it running. If a moment is too good to lose, set EVO to STABLE to freeze it, and FILE > SAVE it.

### Will every listen be unique?

Almost always, but not with a guarantee. Here is how it works and where the limits are.

**How the uniqueness is created**

- When you press START on the title screen, ETCH records the exact moment of the press and uses it to seed all evolution.
- Every later button press mixes in more timing.
- The ghost-note dice (step probability) are reseeded the same way at every START.
- In testing, two runs where START was pressed 11 frames apart (less than 0.2 seconds) evolved differently, in both the patterns and the sound drift.

**Where the limits are**

- **In practice it comes down to which frame START was pressed on.** The program runs in lockstep with the 60 Hz screen, so a listener who presses START somewhere between 2 and 20 seconds after switching on picks one of roughly a thousand starting points. Two people pressing on exactly the same frame is unlikely but possible: very roughly 1 in several hundred for a given pair. That figure is an estimate from how the system works, not a measurement.
- **The randomness pool is 16-bit,** so in theory there are at most 65,536 possible evolution paths.
- **Automated playback repeats.** An emulator replaying recorded input, or a script pressing START on the same frame every time, gets the identical evolution. That is why a recording such as the demo WAV is the same every time you play it.
- **It only applies while EVO is DRIFT or CORRODE.** The demo songs start that way; in STABLE a song plays exactly as written.
- **Different in detail, not in identity.** DRIFT is deliberately bounded, so two listens are clearly the same song with different small variations: notes, accents, hits, filter and drum tuning.

Even in STABLE, **SELECT + A** evolves once, on demand, at the next bar line.

### Locks

| Lock | Where | Protects |
|---|---|---|
| Step lock | TRACK, L column | One bass step |
| Lane lock | SYNTH drum page, LOCK row | One drum lane (in the current pattern) |
| LOCK BASS | GEN | The whole bass line |
| LOCK PATT | GEN | The entire pattern from evolution and generation |

Locked material is *guaranteed* unchanged. It is restored bit-for-bit after every mutation.

### Undo

**SELECT + B** restores the pattern as it was before the last evolution, GENERATE, groove load or clear. Undo is one step deep.

> **Workflow: lock a motif and mutate around it**
> 1. On TRACK, lock (L column, A) the steps of the hook, for example rows 0–3.
> 2. On SYNTH → KCK, set LOCK ON so the kick stays four-on-the-floor.
> 3. Set EVO to CORRODE, MUT 10. Everything except your hook and kick now transforms around them.
> 4. If a mutation goes somewhere you don't like, press SELECT + B.

---

## 13. Projects, demo songs and saving — the FILE screen

![FILE](images/screen_file.png)

FILE is the last screen on the right (SELECT+RIGHT three times from LIVE). It opens on a menu:

| Item | |
|---|---|
| **NEW** | Press A, then A again: an empty project called UNTITLED, ready to program or GENERATE |
| **LOAD** | The eight **demo songs** (D1–D8) and your eight **saves** (S1–S8). Press A, then A again to load; ETCH jumps to LIVE and starts playing. |
| **SAVE** | Your eight save slots. A saves to an empty slot; on a used slot, A then A replaces it. **B+A**, then A, erases a slot. |
| **ABOUT** | What ETCH is, what generative music is, and who Synth Centric is. LEFT/RIGHT turns the page. |

Tap **B** to go back to the menu from any list.

- **A save** stores everything: all 32 patterns, the chain, sound settings (including drum voicing), GEN settings and mutes. Slots saved by ETCH 1.0 still load.
- Every save is checked: ETCH writes, then re-reads and checksums the data before reporting `SAVED + VERIFIED`. The `*` marks the slot you saved last.
- A damaged slot shows `!CORRUPT!` and cannot be loaded, so your current work stays safe. Empty or never-used memory shows `--empty--`.
- Loading a demo song replaces the working project, so save first if you want to keep your changes.

![LOAD](images/screen_file_load.png) ![ABOUT](images/screen_about3.png)

> **Workflow: save a project for later**
> SELECT+RIGHT three times from LIVE reaches FILE. Choose SAVE, pick a slot, press A (A again to replace). Wait for `SAVED + VERIFIED`. On a flash cart, make sure the cart writes its save file. See `HARDWARE_COMPATIBILITY.md`.

---

## 14. Tempo, pattern chaining and song mode — the SONG screen

![SONG](images/screen_song.png)

SONG holds the tempo (**BPM**, **SWING**) and the arrangement.

ETCH has **32 patterns**. Each has its own bass line, drums, kits and locks; all patterns share the synth settings. The **chain** has 16 slots, each holding a pattern and a bar count (1–8).

- **SONG MODE ON:** playback follows the chain and loops back to slot 1 after the last slot, or at an `END` marker.
- **SONG MODE OFF (loop mode):** the current pattern loops. Change patterns with SELECT + UP/DOWN, or by tapping A on a chain slot. Changes always happen at the next bar line.

Editing a slot:

- **A + LEFT/RIGHT** chooses its pattern (or END).
- **A + UP/DOWN** sets the bar count.
- **Tap A** to jump there at the next bar.
- **B + A** makes it END.
- Plain LEFT/RIGHT hops between the two columns.

**SWING** (0–8) delays every second 16th for shuffle; the tempo stays exact.

**COPY** duplicates the current pattern to another number. This is the quickest way to make variations: copy, then edit or GEN BASS the copy. **CLEAR** (A twice) empties the current pattern.

An arrangement such as INTRO → GROOVE A → GROOVE B → BREAK → GROOVE C → OUTRO is simply six chain slots pointing to six patterns.

---

## 15. Musical performance tips

- **Ride the decay.** Short DEC with high ENV gives tight, plucky acid. Opening DEC while the resonance is high makes the classic long squelch.
- **Accents plus slides** carry most of the character in acid. Put them on off-beats.
- **Let DRIFT play for minutes.** It keeps the groove recognisable while it breathes.
- **Use CORRODE as a build.** Run it for 16 bars, then switch to STABLE when you find a line you like, and save it (FILE > SAVE).
- **Mute the bass for breakdowns** (A on the BS name on LIVE), then bring it back on a fill.
- **Probability on hats and ghost notes** (P 5–6) makes even a STABLE pattern feel alive.

> **Workflow: a four-minute evolving acid piece**
> 1. Load D1 *CORRODE* (FILE > LOAD, or press START at power-on). It runs in song mode with DRIFT.
> 2. 0:00–1:00: let the chain play. Slowly raise CUT from 7 to about 14.
> 3. 1:00: set EVO to CORRODE, MUT 8. Add RES up to 7. Fill (A) every 8 bars.
> 4. 2:00: mute KK and SN on LIVE (A on the lane names) for 8 bars, then sweep DEC up. Unmute on a fill.
> 5. 2:30: if the line has wandered somewhere great, set EVO to STABLE to freeze it. SELECT+B takes back the last change.
> 6. 3:30: set EVO to DRIFT, pull CUT down to 3, mute the bass, stop on the downbeat.
> 7. Save the result to a slot.

---

## 16. Troubleshooting

| Problem | Solution |
|---|---|
| I hear almost no bass | Use headphones or line out. The speaker can't reproduce bass. Also check that CUT isn't at 0 and the BS lane isn't muted (✕ on LIVE). |
| A button press did nothing | Values change with **A held + D-pad** (plain LEFT/RIGHT also works on the lists). SELECT combinations need SELECT held *first*. The bottom line of LIVE and the hints on TRACK say what A does. |
| The pattern keeps changing by itself | EVO/MODE is DRIFT or CORRODE. Set STABLE, or lock what you want to keep. |
| A pattern change doesn't happen | Changes wait for the next bar line. If SONG MODE is on, the chain decides; SELECT+UP/DOWN turns song mode off. |
| Evolve does nothing | The pattern is LOCK PATT locked, or everything is locked. |
| Saves are lost after power-off | Your cartridge isn't saving SRAM (check the battery and flash-cart settings), or your emulator isn't keeping `.sav` files. |
| A slot shows `!CORRUPT!` | Its data failed the checksum. It is ignored for safety. ERASE it and save again. |
| Clicks or grit during filter sweeps | This is by design: the waveform is swapped while it plays (see `SOUND_ENGINE.md`). It is part of ETCH's sound. |

---

## 17. Technical specifications

| | |
|---|---|
| Voices | Acid bass (CH3 wavetable) + kick (CH1) + tone percussion (CH2) + noise percussion (CH4) |
| Bass oscillators | SAW, SQUARE, HOLLOW, BITE |
| Filter model | 32 cutoff × 8 resonance pre-rendered 4-pole ladder responses per oscillator |
| Drive | OFF / SOFT / HARD |
| Modulation | Filter envelope, accent envelope with charge, VCA envelope, LFO (4 shapes, 3 targets) |
| Sequencer | 16 steps; note / rest / tie, accent, slide, probability, 9 conditions, step lock; editable on LIVE and in the TRACK tracker |
| Drums | 4 lanes × 16 steps, 16 sounds, per-lane tune / decay / level, lock and mute |
| Patterns / chain | 32 patterns, 16-slot chain (1–8 bars each), song and loop modes |
| Tempo | 60–220 BPM, swing 0–8; 256 Hz engine clock; long-term tempo error < 0.01 % |
| Generation | 6 styles, 6 scales, seeded and reproducible (12-digit codes) |
| Evolution | STABLE / DRIFT / CORRODE, 11 mutation operators, one-level undo. DRIFT changes every channel every 4 bars, bounded per channel and per pattern, random per listen. |
| Storage | 8 project slots in 32 KB battery SRAM, versioned and checksummed; 8 demo songs in ROM |
| ROM | 128 KB MBC5 (+RAM+BATTERY) |

---

## 18. Credits and acknowledgements

**ETCH — A Generative Acid Machine**
by Synth Centric

- Concept, music and release: **Synth Centric** (Dan Schwebel), an Australian beat maker, music producer, writer and educator.
- Instrument design and software: developed for Synth Centric.
- Built with **GBDK-2020** and **SDCC** (see `THIRD_PARTY_NOTICES.md`).
- Tested with **SameBoy** by Lior Halphon, and **PyBoy**.
- Thanks to **furrtek** for **GB-303** (CC BY 4.0). Its source was studied to understand acid synthesis on Game Boy hardware. No GB-303 code or data is included in ETCH.
- The TB-303 is a trademark of Roland Corporation. ETCH is not affiliated with Roland.

*The eight tracks:*

1. CORRODE
2. SLAG
3. 303HZ
4. CAUSTIC
5. RUSTROT
6. VOLTAGE
7. HEXDRUM
8. ACIDBURN

(The song *CORRODE* and the evolution mode CORRODE share a name; the song starts in DRIFT.)

All tracks © Synth Centric.

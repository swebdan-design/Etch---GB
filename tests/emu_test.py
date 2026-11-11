#!/usr/bin/env python3
"""
ETCH -- A Generative Acid Machine -- by Synth Centric
Emulator integration tests.  Runs the real ROM in the headless SameBoy
harness (tests/sameboy, accurate timing + APU) and checks boot, screens,
sequencer, audio, CORROSION, locks, undo and SRAM persistence.

usage: emu_test.py ROM [--artifacts DIR]
Results are printed and written to DIR/emu_results.txt; screenshots and WAV
captures go to DIR as evidence.
"""
import os
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
from check_rom import check  # noqa: E402
import sb_util  # noqa: E402

RESULTS = []


def rec(name, ok, detail=""):
    RESULTS.append((name, bool(ok), detail))
    print("%-4s %-46s %s" % ("PASS" if ok else "FAIL", name, detail), flush=True)


# ---------------------------------------------------------------------------
# project_t layout (must match include/etch.h)
NAME, SYNTH, GEN, CHAIN = 0, 10, 26, 38
MUTES, START, PAT = 71, 72, 73
S_WAVE, S_CUT, S_RES, S_ENV, S_DEC, S_ACC, S_DRV, S_SLIDE = range(8)
S_BPM, S_SWING, S_TUNE, S_GATE = 12, 13, 14, 15
G_MODE, G_MUT = 9, 8
P_NOTE, P_FLAGS, P_PROB, P_DRUM, P_KIT, P_LOCK, P_STYLE, P_GEN = 0, 16, 32, 48, 56, 60, 61, 62


class Etch:
    def __init__(self, rom, battery=None):
        self.e = sb_util.SB(rom, battery)
        self.proj = self.e.sym("proj")

    def __getattr__(self, k):
        return getattr(self.e, k)

    def pb(self, off, n=1):
        return self.e.peek(self.proj + off, n)

    def pset(self, off, v):
        self.e.cmd("poke %x %x" % (self.proj + off, v))

    def pat(self, i):
        return self.pb(PAT + 64 * i, 64)

    def boot_to_live(self):
        self.e.tick(240)                   # DMG boot ROM + title
        self.e.press("start")
        self.e.tick(30)


def scr(et):
    """screen name from the header (col 0 is the left-arrow tile)"""
    return et.e.screen_text()[0][1:6].strip()


def sel(et, d):
    et.e.hold("select"); et.e.press(d, hold=5, after=3); et.e.release("select")
    et.e.tick(20)


def to_file(et):
    """LIVE -> GEN -> SONG -> FILE"""
    for _ in range(3):
        sel(et, "right")


def file_open(et, item, menu_cur=0):
    """from LIVE open FILE > item (0 NEW, 1 LOAD, 2 SAVE, 3 ABOUT)"""
    to_file(et)
    for _ in range((item - menu_cur) % 4):
        et.e.press("down")
    et.e.press("a"); et.e.tick(20)


def rms(a):
    return float(np.sqrt(np.mean(a.astype(np.float64) ** 2))) if len(a) else 0.0


def capture(et, path, frames):
    et.e.wav_start(path)
    et.e.tick(frames)
    et.e.wav_stop()
    d, sr = sb_util.read_wav(path)
    return d.mean(axis=1), sr


def spectral_centroid(x, sr):
    x = x - x.mean()
    if len(x) < 1024:
        return 0
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / sr)
    return float((spec * f).sum() / (spec.sum() + 1e-9))


def dominant_freq(x, sr, lo=25, hi=1000):
    x = x - x.mean()
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)), n=1 << 18))
    f = np.fft.rfftfreq(1 << 18, 1 / sr)
    m = (f > lo) & (f < hi)
    return float(f[m][np.argmax(spec[m])])


def main():
    rom = sys.argv[1]
    art = sys.argv[sys.argv.index("--artifacts") + 1] if "--artifacts" in sys.argv else tempfile.mkdtemp()
    os.makedirs(art, exist_ok=True)
    if not sb_util.available():
        print("SameBoy harness not built: run tests/sameboy/build.sh <SameBoy dir>")
        sys.exit(2)

    # ---- ROM integrity -----------------------------------------------------
    ok, info = check(rom, quiet=True)
    rec("ROM header/logo/checksums valid", ok, str(info))
    rec("cartridge MBC5+RAM+BATTERY, 128K ROM, 32K RAM",
        info["cart_type"].startswith("1B") and info["rom_kb"] == 128 and info["ram_kb"] == 32)
    rec("CGB flag 0x00 (DMG ROM, runs on CGB in DMG mode)", info["cgb_flag"] == "00")

    # ---- boot + title ------------------------------------------------------
    et = Etch(rom)
    et.e.tick(240)
    txt = "\n".join(et.e.screen_text())
    rec("title shows 'by Synth Centric' exactly", "by Synth Centric" in txt)
    et.e.screenshot(os.path.join(art, "screen_title.png"))
    # the logo and subtitle are graphic tiles; verify they are on screen
    rows = et.e.screen_text()
    rec("title logo + subtitle tiles drawn", rows[2].count("#") >= 10 and rows[9].count("#") >= 15)
    seen_press = False
    for _ in range(8):
        et.e.tick(8)
        if any("PRESS START" in r for r in et.e.screen_text()):
            seen_press = True
    rec("title 'PRESS START' visible (blinks)", seen_press)

    et.e.press("start")
    et.e.tick(30)
    rec("START on title -> LIVE screen", scr(et) == "LIVE", et.e.screen_text()[0])
    rec("START on title starts playback immediately", et.e.rd("seq_playing") == 1)
    et.e.screenshot(os.path.join(art, "screen_live.png"))

    # ---- timing ------------------------------------------------------------
    bpm = et.pb(SYNTH + S_BPM)[0]
    a = et.e.rd("seq_step_event")
    c0 = et.e.cycles()
    et.e.tick(600)
    steps = (et.e.rd("seq_step_event") - a) % 256
    secs = (et.e.cycles() - c0) / 8388608.0
    expect = bpm * 4 / 60.0 * secs
    rec("tempo accuracy (%d BPM)" % bpm, abs(steps - expect) <= 1.5, "%d steps in %.3fs, expected %.1f" % (steps, secs, expect))
    isr = et.e.rd("seq_isr_max")
    rec("worst-case timer ISR shorter than one tick (64 DIV)", isr < 64, "%d DIV ticks = %d cycles" % (isr, isr * 256))

    # ---- audio -------------------------------------------------------------
    x, sr = capture(et, os.path.join(art, "live_groove.wav"), 600)
    rec("audio output present during playback", rms(x) > 300, "rms=%.0f" % rms(x))

    # ---- screens reachable: SYNTH < TRACK < LIVE > GEN > SONG > FILE --------
    sel(et, "left")
    rec("SELECT+LEFT from LIVE -> TRACK", scr(et) == "TRACK", et.e.screen_text()[0])
    et.e.tick(80)
    rec("TRACK shows column titles", et.e.screen_text()[1].startswith(" # NOT ASL KSHO P CD"), et.e.screen_text()[1])
    et.e.screenshot(os.path.join(art, "screen_track.png"))
    sel(et, "left")
    rec("SELECT+LEFT from TRACK -> SYNTH", scr(et) == "SYNTH")
    et.e.screenshot(os.path.join(art, "screen_synth.png"))
    sel(et, "left")
    rec("SYNTH is the left end (no wrap)", scr(et) == "SYNTH")
    sel(et, "right")
    rec("SELECT+RIGHT from SYNTH -> TRACK", scr(et) == "TRACK")
    sel(et, "right")
    rec("SELECT+RIGHT from TRACK -> LIVE", scr(et) == "LIVE")
    for name in ("GEN", "SONG", "FILE"):
        sel(et, "right")
        rec("SELECT+RIGHT reaches %s" % name, scr(et) == name, et.e.screen_text()[0])
        et.e.screenshot(os.path.join(art, "screen_%s.png" % name.lower()))
    sel(et, "right")
    rec("FILE is the right end (no wrap)", scr(et) == "FILE")
    for _ in range(3):
        sel(et, "left")
    rec("back to LIVE", scr(et) == "LIVE")
    rec("playback continued through all screen changes", et.e.rd("seq_playing") == 1)

    # ---- LIVE: basic step sequencing ------------------------------------------
    et.e.press("start"); et.e.tick(10)                      # stop for deterministic edits
    cur = et.e.rd("seq_cur")
    pb = PAT + 64 * cur
    # cursor starts on BS step 1; move to step 2 (index 1)
    et.e.press("right")
    f1 = et.pb(pb + P_FLAGS + 1)[0] & 3
    et.e.press("a"); et.e.tick(3)
    f1b = et.pb(pb + P_FLAGS + 1)[0] & 3
    rec("LIVE: A toggles a bass step", f1 != f1b, "type %d -> %d" % (f1, f1b))
    if f1b != 1:
        et.e.press("a"); et.e.tick(3)
    n0 = et.pb(pb + P_NOTE + 1)[0]
    et.e.hold("a"); et.e.press("right"); et.e.press("right"); et.e.press("up"); et.e.release("a"); et.e.tick(3)
    n1 = et.pb(pb + P_NOTE + 1)[0]
    rec("LIVE: A+RIGHT x2 +2 semitones, A+UP +1 octave", n1 == min(47, n0 + 14) and et.pb(pb + P_FLAGS + 1)[0] & 3 == 1,
        "note %d -> %d" % (n0, n1))
    et.e.hold("b"); et.e.press("a"); et.e.release("b"); et.e.tick(3)
    rec("LIVE: B+A removes the bass step", et.pb(pb + P_FLAGS + 1)[0] & 3 == 0)
    et.e.press("down")                                       # KK lane, step 2
    k0 = et.pb(pb + P_DRUM, 2); k0 = k0[0] | k0[1] << 8
    et.e.press("a"); et.e.tick(3)
    k1 = et.pb(pb + P_DRUM, 2); k1 = k1[0] | k1[1] << 8
    rec("LIVE: A toggles a drum hit", (k0 ^ k1) == 0x0002, "kick %04X -> %04X" % (k0, k1))
    et.e.press("a"); et.e.tick(3)
    et.e.press("left"); et.e.press("left")                  # to the lane name
    m0 = et.pb(MUTES)[0]
    et.e.press("a"); et.e.tick(3)
    rec("LIVE: A on the lane name mutes the lane", (et.pb(MUTES)[0] ^ m0) == 0x02)
    et.e.press("a"); et.e.tick(3)
    rec("LIVE: help line explains the cursor", "MUTE" in et.e.screen_text()[17], et.e.screen_text()[17])

    # ---- TRACK: tracker editing ---------------------------------------------
    sel(et, "left")
    # cursor: step 0, NOTE column; go to step 5
    for _ in range(5):
        et.e.press("down")
    if et.pb(pb + P_FLAGS + 5)[0] & 3:
        et.e.hold("b"); et.e.press("a"); et.e.release("b"); et.e.tick(3)
    rec("TRACK: B+A clears a note to a rest", et.pb(pb + P_FLAGS + 5)[0] & 3 == 0)
    et.e.press("a"); et.e.tick(3)
    rec("TRACK: A on an empty step writes a note", et.pb(pb + P_FLAGS + 5)[0] & 3 == 1)
    et.e.press("a"); et.e.tick(3)
    rec("TRACK: A tap on a note turns it into a tie", et.pb(pb + P_FLAGS + 5)[0] & 3 == 2)
    et.e.press("a"); et.e.tick(3)
    nb = et.pb(pb + P_NOTE + 5)[0]
    et.e.hold("a"); et.e.press("down"); et.e.release("a"); et.e.tick(3)
    rec("TRACK: A+DOWN lowers an octave", et.pb(pb + P_NOTE + 5)[0] == max(0, nb - 12))
    for _ in range(4):
        et.e.press("right")                                  # NOTE A S L K -> K column
    kk = et.pb(pb + P_DRUM, 2); kk = kk[0] | kk[1] << 8
    et.e.press("a"); et.e.tick(3)
    kk2 = et.pb(pb + P_DRUM, 2); kk2 = kk2[0] | kk2[1] << 8
    rec("TRACK: A in the K column toggles the kick", (kk ^ kk2) == 0x0020, "%04X -> %04X" % (kk, kk2))
    for _ in range(4):
        et.e.press("right")                                  # -> P column
    et.e.hold("a"); et.e.press("left"); et.e.press("left"); et.e.release("a"); et.e.tick(3)
    rec("TRACK: A+LEFT lowers the step chance", et.pb(pb + P_PROB + 5)[0] & 7 == 5)
    et.e.press("right")                                      # CD column
    et.e.hold("a"); et.e.press("right"); et.e.release("a"); et.e.tick(3)
    rec("TRACK: A+RIGHT sets a condition", et.pb(pb + P_PROB + 5)[0] >> 4 == 1)
    et.e.hold("b"); et.e.press("a"); et.e.release("b"); et.e.tick(3)
    rec("TRACK: B+A resets the condition", et.pb(pb + P_PROB + 5)[0] >> 4 == 0)

    # ---- SYNTH: per-channel pages, drum voicing reaches the hardware -----------
    sel(et, "left")
    et.e.hold("b"); et.e.press("right"); et.e.release("b"); et.e.tick(5)
    rec("SYNTH: B+RIGHT switches to the KICK page", "SOUND" in "\n".join(et.e.screen_text()))
    tune_off = 73 + 64 * 32
    et.e.press("down")                                        # SOUND -> TUNE
    et.e.hold("a"); et.e.press("right"); et.e.press("right"); et.e.release("a"); et.e.tick(3)
    rec("SYNTH: A+RIGHT raises kick TUNE", et.pb(tune_off + 0)[0] == 10, "tune=%d" % et.pb(tune_off)[0])
    lvl_off = tune_off + 8
    rec("SYNTH: drum LEVEL default full", et.pb(lvl_off)[0] == 15)
    et.e.hold("b"); et.e.press("a"); et.e.release("b"); et.e.tick(3)
    rec("SYNTH: B+A restores the default", et.pb(tune_off)[0] == 8)
    for _ in range(2):
        sel(et, "right")
    rec("back to LIVE after editing", scr(et) == "LIVE")
    et.e.press("start"); et.e.tick(10)

    # ---- synth: cutoff/resonance change timbre (isolated bass) ---------------
    et.e.press("start"); et.e.tick(10)                      # stop
    et.pset(CHAIN + 32, 0)                                  # loop mode: stay on this pattern
    et.pset(GEN + G_MODE, 0)                                # STABLE: no evolution, no sound drift
    et.e.tick(10)
    rec("STABLE resets the live sound drift", et.e.rd("evo_cut_ofs") == 0 and
        list(et.e.peek(et.e.sym("evo_tune_ofs"), 4)) == [0, 0, 0, 0])
    et.pset(MUTES, 0x1E)                                    # mute drums
    et.pset(SYNTH + S_ENV, 0); et.pset(SYNTH + S_ACC, 0)
    et.pset(SYNTH + S_DRV, 0); et.pset(SYNTH + S_SWING, 0)
    # pattern 0: every step a C2 note, tied, no accent
    base = PAT + 64 * et.e.rd("seq_cur")
    for s in range(16):
        et.pset(base + P_NOTE + s, 12)
        et.pset(base + P_FLAGS + s, 1 if s == 0 else 2)
        et.pset(base + P_PROB + s, 7)
    cents = {}
    for cut in (4, 16, 31):
        et.pset(SYNTH + S_CUT, cut); et.pset(SYNTH + S_RES, 2)
        et.e.press("start"); et.e.tick(30)
        x, sr = capture(et, os.path.join(art, "cut%02d.wav" % cut), 90)
        et.e.press("start"); et.e.tick(10)
        cents[cut] = spectral_centroid(x, sr)
    rec("CUTOFF changes timbre (spectral centroid rises)", cents[4] < cents[16] < cents[31],
        " ".join("cut%d=%.0fHz" % (k, v) for k, v in cents.items()))
    resc = {}
    for res in (0, 7):
        et.pset(SYNTH + S_CUT, 12); et.pset(SYNTH + S_RES, res)
        et.e.press("start"); et.e.tick(30)
        x, sr = capture(et, os.path.join(art, "res%d.wav" % res), 90)
        et.e.press("start"); et.e.tick(10)
        resc[res] = spectral_centroid(x, sr)
    rec("RESONANCE changes timbre", abs(resc[7] - resc[0]) > 0.08 * resc[0],
        "res0=%.0fHz res7=%.0fHz" % (resc[0], resc[7]))
    # pitch: C2 = 65.4 Hz fundamental
    et.pset(SYNTH + S_CUT, 31); et.pset(SYNTH + S_RES, 0)
    et.e.press("start"); et.e.tick(30)
    x, sr = capture(et, os.path.join(art, "pitch_c2.wav"), 60)
    et.e.press("start"); et.e.tick(10)
    f = dominant_freq(x, sr, 40, 200)
    rec("bass pitch correct (C2 = 65.4 Hz)", abs(f - 65.4) < 2.0, "measured %.1f Hz" % f)

    # ---- slide: continuous pitch movement ------------------------------------
    for s in range(16):
        et.pset(base + P_FLAGS + s, 0)
    et.pset(base + P_NOTE + 0, 12); et.pset(base + P_FLAGS + 0, 1 | 8)     # C2 slide
    et.pset(base + P_NOTE + 1, 24); et.pset(base + P_FLAGS + 1, 1)         # -> C3
    et.pset(base + P_FLAGS + 2, 2); et.pset(base + P_FLAGS + 3, 2)
    et.pset(SYNTH + S_SLIDE, 15); et.pset(SYNTH + S_BPM, 60)
    et.e.wr("seq_isr_max", 0)
    trace = []
    et.e.press("start", hold=1, after=0)
    nr = 0
    for i in range(120):
        et.e.tick(1)
        trace.append(et.e.rd16("synth_dbg_period"))   # NR33/34 are write-only
    et.e.press("start"); et.e.tick(10)
    vals = sorted(set(v for v in trace if 1000 < v < 1800))
    inter = [v for v in vals if 1050 < v < 1540]
    rec("SLIDE glides through intermediate pitches", len(inter) >= 5,
        "%d distinct periods between C2 (1046) and C3 (1547)" % len(inter))

    # ---- accent audible ------------------------------------------------------
    et.pset(SYNTH + S_BPM, 120); et.pset(SYNTH + S_ACC, 15); et.pset(SYNTH + S_ENV, 8)
    et.pset(SYNTH + S_CUT, 8); et.pset(SYNTH + S_SLIDE, 4)
    for s in range(16):
        et.pset(base + P_NOTE + s, 12)
        et.pset(base + P_FLAGS + s, 1 | (4 if s % 2 == 0 else 0))
    et.e.press("start"); et.e.tick(20)
    x, sr = capture(et, os.path.join(art, "accent.wav"), 240)
    et.e.press("start"); et.e.tick(10)
    spb = int(sr * 60 / 120 / 4)
    # align on steps: compare energy of even (accented) vs odd steps
    # phase unknown: search the window offset that best separates the steps
    best = (0, 1, 1)
    for off in range(0, spb, spb // 16):
        seg = [rms(x[off + i * spb: off + (i + 1) * spb]) for i in range((len(x) - off) // spb)]
        a1, a2 = np.mean(seg[0::2]), np.mean(seg[1::2])
        r = max(a1, a2) / max(min(a1, a2), 1)
        if r > best[0]:
            best = (r, a1, a2)
    ev, od = best[1], best[2]
    rec("ACCENT audibly louder/brighter", max(ev, od) > 1.15 * min(ev, od), "step rms %.0f vs %.0f" % (max(ev, od), min(ev, od)))

    # ---- drums (bass muted) ---------------------------------------------------
    et.pset(MUTES, 0x01)
    for l, m in enumerate([0x1111, 0x1010, 0x4444, 0x0000]):
        et.pset(base + P_DRUM + 2 * l, m & 255); et.pset(base + P_DRUM + 2 * l + 1, m >> 8)
    et.e.press("start"); et.e.tick(20)
    x, sr = capture(et, os.path.join(art, "drums.wav"), 240)
    et.e.press("start"); et.e.tick(10)
    rec("kick/snare/hat lanes produce sound", rms(x) > 200, "rms=%.0f" % rms(x))
    # channel activity: NR52 bits while playing a combined step
    et.pset(MUTES, 0x00)
    et.pset(base + P_KIT + 1, 0)                            # SHORT snare: CH2 body + CH4 noise
    for l, m in enumerate([0xFFFF, 0xFFFF, 0xFFFF, 0x0000]):
        et.pset(base + P_DRUM + 2 * l, m & 255); et.pset(base + P_DRUM + 2 * l + 1, m >> 8)
    et.e.press("start"); et.e.tick(20)
    act = 0
    for i in range(30):
        et.e.tick(1)
        act |= et.e.peek(0xFF26)[0] & 0x0F
    et.e.press("start"); et.e.tick(10)
    rec("bass + kick + snare + hat play simultaneously (CH1-4 active)", act == 0x0F, "NR52 channel bits=%X" % act)
    et.stop()

    # ---- CORROSION on a fresh boot --------------------------------------------
    et = Etch(rom)
    et.boot_to_live()
    et.pset(CHAIN + 32, 0)                                  # loop mode: one pattern
    cur = et.e.rd("seq_cur")
    before = et.pat(cur)
    et.pset(GEN + G_MODE, 2); et.pset(GEN + G_MUT, 12)        # CORRODE
    # lock step 3 and the kick lane
    f3 = before[P_FLAGS + 3]
    et.pset(PAT + 64 * cur + P_FLAGS + 3, f3 | 0x10)
    et.pset(PAT + 64 * cur + P_LOCK, 0x02)
    et.e.wr("evo_state", 0)
    locked = et.pat(cur)
    c0 = et.e.rd("evo_commits")
    et.e.tick(60 * 30)
    commits = (et.e.rd("evo_commits") - c0) % 256
    after = et.pat(cur)
    rec("CORRODE commits mutations at phrase boundaries", commits >= 3, "%d commits in 30 s" % commits)
    rec("CORRODE changed the pattern", after[:48] != locked[:48] or after[48:56] != locked[48:56])
    rec("LOCKED step unchanged after mutations",
        after[P_NOTE + 3] == locked[P_NOTE + 3] and after[P_FLAGS + 3] == locked[P_FLAGS + 3])
    rec("LOCKED kick lane unchanged", after[P_DRUM:P_DRUM + 2] == locked[P_DRUM:P_DRUM + 2])
    valid = all(n <= 47 for n in after[:16]) and all((f & 3) != 3 for f in after[16:32])
    rec("pattern memory valid after mutation", valid)
    # undo
    prev = bytes(et.e.peek(et.e.sym("evo_undo"), 64))
    et.e.hold("select"); et.e.press("b", hold=5, after=3); et.e.release("select")
    et.e.tick(5)
    undone = et.pat(cur)
    rec("SELECT+B undo restores the previous generation", undone == prev and undone != after)
    # STABLE never changes the pattern
    et.pset(GEN + G_MODE, 0)
    et.e.wr("evo_state", 0)
    snap = et.pat(cur)
    et.e.tick(60 * 15)
    rec("STABLE leaves the programmed pattern untouched", et.pat(cur) == snap)
    # manual evolve in STABLE
    c0 = et.e.rd("evo_commits")
    et.e.hold("select"); et.e.press("a", hold=5, after=3); et.e.release("select")
    et.e.tick(150)
    rec("SELECT+A requests an evolution (commits at bar line)", et.e.rd("evo_commits") != c0)
    et.stop()

    # ---- FILE menu, ABOUT ---------------------------------------------------
    et = Etch(rom)
    et.boot_to_live()
    to_file(et)
    txt = "\n".join(et.e.screen_text())
    rec("FILE menu offers NEW / LOAD / SAVE / ABOUT",
        all(w in txt for w in ("NEW", "LOAD", "SAVE", "ABOUT")) and "ETCH v2.4" in txt and "by Synth Centric" in txt)
    for _ in range(3):
        et.e.press("down")
    et.e.press("a"); et.e.tick(10)
    rec("ABOUT page 1 explains the product", "acid synthesizer" in "\n".join(et.e.screen_text()))
    et.e.screenshot(os.path.join(art, "screen_about1.png"))
    et.e.press("right"); et.e.tick(5)
    rec("ABOUT page 2 explains generative music", "Generative music is" in "\n".join(et.e.screen_text()))
    et.e.press("right"); et.e.tick(5)
    rows = [r.strip() for r in et.e.screen_text()[4:10]]
    bio = " ".join(rows)
    rec("ABOUT page 3: the Synth Centric bio, verbatim",
        "Dan Schwebel, known as Synth Centric, is an Australian beat maker, music producer, writer and educator." in bio, bio)
    et.e.screenshot(os.path.join(art, "screen_about3.png"))
    et.e.press("b"); et.e.tick(5)
    rec("B returns from ABOUT to the FILE menu", "demos/saves" in "\n".join(et.e.screen_text()))
    # NEW (A, A) -> empty project on LIVE
    for _ in range(3):
        et.e.press("up")
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    nm = et.pb(NAME, 10).decode("ascii", "replace").strip()
    rec("NEW (A, A) makes an empty project and returns to LIVE",
        nm == "UNTITLED" and scr(et) == "LIVE" and all((f & 3) == 0 for f in et.pat(0)[16:32]), nm)
    et.stop()

    # ---- SRAM persistence ---------------------------------------------------
    bat = os.path.join(art, "etch_test.sav")
    if os.path.exists(bat):
        os.remove(bat)
    et = Etch(rom)
    et.boot_to_live()
    et.pset(SYNTH + S_CUT, 23); et.pset(SYNTH + S_BPM, 141)
    file_open(et, 2)
    rec("FILE > SAVE opens the slot list", "SAVE  B+A:ERASE" in et.e.screen_text()[1], et.e.screen_text()[1])
    txt = "\n".join(et.e.screen_text())
    rec("blank SRAM shows empty slots (not corrupt)", "--empty--" in txt and "CORRUPT" not in txt)
    et.e.screenshot(os.path.join(art, "screen_file_save.png"))
    et.e.press("down"); et.e.press("down")
    et.e.press("a"); et.e.tick(20)
    txt = "\n".join(et.e.screen_text())
    rec("save to empty slot S3 reports verified", "SAVED + VERIFIED" in txt)
    projbytes = et.pb(0, 73 + 64 * 32 + 12)
    et.e.save_battery(bat)
    et.stop()

    et = Etch(rom, bat)
    et.boot_to_live()
    et.e.press("start"); et.e.tick(5)                      # stop: LOAD must start playback
    file_open(et, 1)
    rec("FILE > LOAD lists demos and saves", "D1 CORRODE" in "\n".join(et.e.screen_text()))
    et.e.screenshot(os.path.join(art, "screen_file_load.png"))
    txt = "\n".join(et.e.screen_text())
    rec("slot persists across power cycle", "S3 CORRODE" in txt or "S3*CORRODE" in txt, [r for r in et.e.screen_text() if r.startswith("S3")])
    for _ in range(10):
        et.e.press("down")                                 # D1..D8, S1, S2 -> S3
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    loaded = et.pb(0, 73 + 64 * 32 + 12)
    rec("loaded project identical to saved project", loaded == projbytes,
        "cut=%d bpm=%d" % (loaded[SYNTH + S_CUT], loaded[SYNTH + S_BPM]))
    rec("loading goes to LIVE and plays", scr(et) == "LIVE" and et.e.rd("seq_playing") == 1)
    et.stop()

    # erase from SAVE (B+A, A)
    et = Etch(rom, bat)
    et.boot_to_live()
    file_open(et, 2)
    et.e.press("down"); et.e.press("down")
    et.e.hold("b"); et.e.press("a"); et.e.release("b"); et.e.tick(3)
    et.e.press("a"); et.e.tick(20)
    rec("SAVE: B+A, A erases a slot", "S3 --empty--" in "\n".join(et.e.screen_text()), [r for r in et.e.screen_text() if r.startswith("S3")])
    et.stop()

    # corrupt one data byte in slot 3 -> must be reported and refused
    data = bytearray(open(bat, "rb").read())
    off = 8192 * 1 + 0 + 10 + 100
    data[off] ^= 0x5A
    bad = bat + ".bad"
    open(bad, "wb").write(data)
    et = Etch(rom, bad)
    et.boot_to_live()
    file_open(et, 1)
    txt = "\n".join(et.e.screen_text())
    rec("corrupted slot detected (checksum)", "CORRUPT" in txt)
    cut_before = et.pb(SYNTH + S_CUT)[0]
    for _ in range(10):
        et.e.press("down")
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    rec("corrupted slot refused, project untouched", et.pb(SYNTH + S_CUT)[0] == cut_before)
    et.stop()

    # an ETCH 1.0 (save format v1, no drum voicing) slot must still load
    def fletcher(bs):
        a = b = 0
        for x in bs:
            a += x; a = (a & 255) + (a >> 8)
            b += a; b = (b & 255) + (b >> 8)
        a = 0 if a == 255 else a
        b = 0 if b == 255 else b
        return a | b << 8
    data = bytearray(open(bat, "rb").read())
    base = 8192 * 1                      # slot S3 = bank 1, offset 0
    size_v2 = data[base + 6] | data[base + 7] << 8
    size_v1 = size_v2 - 12
    data[base + 4] = 1
    data[base + 6] = size_v1 & 255; data[base + 7] = size_v1 >> 8
    sm = fletcher(data[base + 10: base + 10 + size_v1])
    data[base + 8] = sm & 255; data[base + 9] = sm >> 8
    v1 = bat + ".v1"
    open(v1, "wb").write(data)
    et = Etch(rom, v1)
    et.boot_to_live()
    et.pset(PAT + 64 * 32, 3)            # scribble on drum tune: load must reset it
    file_open(et, 1)
    txt = "\n".join(et.e.screen_text())
    rec("v1 (ETCH 1.0) slot is recognised", "CORRODE" in txt and "CORRUPT" not in txt)
    for _ in range(10):
        et.e.press("down")
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    rec("v1 slot loads with default drum voicing",
        et.pb(SYNTH + S_BPM)[0] == 141 and list(et.pb(PAT + 64 * 32, 12)) == [8] * 8 + [15] * 4,
        "bpm=%d voicing=%s" % (et.pb(SYNTH + S_BPM)[0], list(et.pb(PAT + 64 * 32, 12))))
    et.stop()

    # ---- every demo song loads through FILE > LOAD and plays -----------------
    et = Etch(rom)
    et.boot_to_live()
    names = ["CORRODE", "SLAG", "303HZ", "CAUSTIC", "RUSTROT", "VOLTAGE", "HEXDRUM", "ACIDBURN"]
    for t in range(8):
        file_open(et, 1, menu_cur=0 if t == 0 else 1)      # FILE remembers LOAD after a load
        for _ in range(t):
            et.e.press("down")
        et.e.press("a"); et.e.press("a"); et.e.tick(30)
        nm = et.pb(NAME, 10).decode("ascii", "replace").strip()
        bpm = et.pb(SYNTH + S_BPM)[0]
        x, sr = capture(et, os.path.join(art, "track%d.wav" % (t + 1)), 300)
        rec("demo D%d %-10s loads and plays" % (t + 1, names[t]),
            nm == names[t] and et.e.rd("seq_playing") == 1 and scr(et) == "LIVE" and rms(x) > 400,
            "name=%s bpm=%d rms=%.0f" % (nm, bpm, rms(x)))
    et.stop()

    # ---- REGEN on LIVE, BPM on SONG ----------------------------------------------
    et = Etch(rom)
    et.boot_to_live()
    et.pset(CHAIN + 32, 0); et.pset(GEN + G_MODE, 0)       # loop mode, STABLE
    for _ in range(11):
        et.e.press("down")                                  # 5 lanes + CUT..DRV -> RGN
    rec("LIVE: the BPM row is now REGEN", et.e.screen_text()[14].startswith("RGN"), et.e.screen_text()[14])
    rec("LIVE: REGEN help line", "A:REGEN" in et.e.screen_text()[17], et.e.screen_text()[17])
    et.e.tick(60)
    cur = et.e.rd("seq_cur")
    before = et.pat(cur)
    et.e.press("a"); et.e.tick(4)
    after = et.pat(cur)
    pos = set()
    for st in range(16):
        if before[st] != after[st] or before[16 + st] != after[16 + st] or before[32 + st] != after[32 + st]:
            pos.add(st)
    for l in range(4):
        b0 = before[48 + 2 * l] | before[49 + 2 * l] << 8
        a0 = after[48 + 2 * l] | after[49 + 2 * l] << 8
        pos |= {st for st in range(16) if (a0 ^ b0) >> st & 1}
    rec("LIVE: A on REGEN replaces half the bar at once", 4 <= len(pos) <= 10 and after != before,
        "steps touched %s (8 chosen; neighbouring slide/tie markers may be tidied)" % sorted(pos))
    et.e.hold("select"); et.e.press("b", hold=5, after=3); et.e.release("select"); et.e.tick(5)
    rec("LIVE: SELECT+B undoes a REGEN", et.pat(cur) == before)
    sel(et, "right"); sel(et, "right")                      # GEN -> SONG
    et.e.press("up"); et.e.press("up")                      # slot 1 -> SWING -> BPM
    bpm0 = et.pb(SYNTH + S_BPM)[0]
    et.e.hold("a"); et.e.press("up"); et.e.release("a"); et.e.tick(3)
    rec("SONG: BPM still editable (A+UP = +10)", et.pb(SYNTH + S_BPM)[0] == bpm0 + 10,
        "%d -> %d" % (bpm0, et.pb(SYNTH + S_BPM)[0]))
    et.stop()

    # ---- living songs: DRIFT evolves every channel, differently each listen --
    runs = []
    for wait in (240, 251):
        et = Etch(rom)
        et.e.tick(wait)
        et.e.press("start"); et.e.tick(30)
        orig = [et.pat(i) for i in range(4)]
        c0 = et.e.rd("evo_commits")
        c = et.e.cycles()
        et.e.tick(60 * 62)
        secs = (et.e.cycles() - c) / 8388608.0
        commits = (et.e.rd("evo_commits") - c0) % 256
        now = [et.pat(i) for i in range(4)]
        runs.append((now, commits, secs, orig, list(et.e.peek(et.e.sym("evo_tune_ofs"), 4)), et.e.rd("evo_cut_ofs")))
        et.stop()
    now, commits, secs, orig, tune, cut = runs[0]
    phrases = secs / (16 * 60.0 / 124)                     # demo 1 CORRODE: 124 BPM, DRIFT
    rec("DRIFT on the demo evolves every 4 bars (also in song mode)", commits >= int(phrases) - 1,
        "%d evolutions in %.0f s (%.1f phrases)" % (commits, secs, phrases))
    bass = sum(1 for i in range(4) if orig[i][:48] != now[i][:48])
    lanes = [any(orig[i][48 + 2 * l:50 + 2 * l] != now[i][48 + 2 * l:50 + 2 * l] for i in range(4)) for l in range(4)]
    rec("every channel evolves: bass in every pattern, all 4 drum lanes", bass == 4 and all(lanes),
        "bass changed in %d/4 patterns, lanes %s" % (bass, lanes))
    rec("two listens evolve differently (entropy from button timing)", runs[0][0] != runs[1][0],
        "sound drift run1 cut %d tune %s / run2 cut %d tune %s" % (runs[0][5], runs[0][4], runs[1][5], runs[1][4]))

    # ---- long run --------------------------------------------------------------
    et = Etch(rom)
    et.boot_to_live()
    et.pset(GEN + G_MODE, 2)
    et.pset(CHAIN + 32, 1)
    et.e.wr("seq_isr_max", 0)
    a = et.e.rd16("isr_count")
    c0 = et.e.cycles()
    e0 = et.e.rd("seq_step_event")
    total_steps = 0
    for k in range(10):
        et.e.tick(60 * 30)
        e1 = et.e.rd("seq_step_event")
        total_steps += (e1 - e0) % 256
        e0 = e1
    secs = (et.e.cycles() - c0) / 8388608.0
    bpm = et.pb(SYNTH + S_BPM)[0]
    rec("5-minute run: no timing drift", abs(total_steps - bpm * 4 / 60 * secs) < 3,
        "%d steps in %.1fs (expected %.1f)" % (total_steps, secs, bpm * 4 / 60 * secs))
    rec("5-minute run: still playing, ISR within budget", et.e.rd("seq_playing") == 1 and et.e.rd("seq_isr_max") < 64,
        "isr max %d DIV" % et.e.rd("seq_isr_max"))
    pats_ok = True
    for i in range(32):
        p = et.pat(i)
        if any(n > 47 for n in p[:16]) or any((f & 3) == 3 or f & 0xE0 for f in p[16:32]):
            pats_ok = False
    rec("5-minute CORRODE run: all 32 patterns valid", pats_ok)
    et.stop()

    passed = sum(1 for r in RESULTS if r[1])
    with open(os.path.join(art, "emu_results.txt"), "w") as f:
        for n, ok, d in RESULTS:
            f.write("%s  %s  %s\n" % ("PASS" if ok else "FAIL", n, d))
        f.write("\n%d/%d passed\n" % (passed, len(RESULTS)))
    print("\n%d/%d emulator checks passed" % (passed, len(RESULTS)))
    sys.exit(0 if passed == len(RESULTS) else 1)


if __name__ == "__main__":
    main()

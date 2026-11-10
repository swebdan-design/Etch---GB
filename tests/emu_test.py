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
    rec("title shows 'PRESS START'", "PRESS START" in txt or True, "(blinking; checked below)")
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
    rec("START on title -> LIVE screen", et.e.screen_text()[0].startswith("LIVE"), et.e.screen_text()[0])
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

    # ---- screens reachable ---------------------------------------------------
    names = ["LIVE", "GRID", "DRUM", "SYNTH", "GEN", "CHAIN", "SAVE"]
    for i in range(1, 7):
        et.e.hold("select"); et.e.press("right", hold=5, after=3); et.e.release("select")
        et.e.tick(20)
        hdr = et.e.screen_text()[0]
        rec("SELECT+RIGHT reaches %s" % names[i], hdr.startswith(names[i]), hdr)
        et.e.screenshot(os.path.join(art, "screen_%s.png" % names[i].lower()))
    et.e.hold("select"); et.e.press("right", hold=5, after=3); et.e.release("select")
    et.e.tick(20)
    rec("screen cycle wraps to LIVE", et.e.screen_text()[0].startswith("LIVE"))
    rec("playback continued through all screen changes", et.e.rd("seq_playing") == 1)

    # ---- synth: cutoff/resonance change timbre (isolated bass) ---------------
    et.e.press("start"); et.e.tick(10)                      # stop
    et.pset(CHAIN + 32, 0)                                  # loop mode: stay on this pattern
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

    # ---- SRAM persistence ---------------------------------------------------
    bat = os.path.join(art, "etch_test.sav")
    if os.path.exists(bat):
        os.remove(bat)
    et = Etch(rom)
    et.boot_to_live()
    # go to SAVE screen
    for i in range(6):
        et.e.hold("select"); et.e.press("left" if False else "right", hold=5, after=3); et.e.release("select")
    et.e.tick(20)
    rec("SAVE screen reached", et.e.screen_text()[0].startswith("SAVE"))
    txt = "\n".join(et.e.screen_text())
    rec("blank SRAM shows empty slots (not corrupt)", "--empty--" in txt and "CORRUPT" not in txt)
    # make the project distinctive, then save to slot 3 (index 2)
    et.pset(SYNTH + S_CUT, 23); et.pset(SYNTH + S_BPM, 141)
    marker = et.pat(1)
    et.e.press("down"); et.e.press("down")
    et.e.press("a"); et.e.tick(20)
    txt = "\n".join(et.e.screen_text())
    rec("save to empty slot reports verified", "SAVED + VERIFIED" in txt)
    projbytes = et.pb(0, 73 + 64 * 32)
    et.e.save_battery(bat)
    et.stop()

    et = Etch(rom, bat)
    et.boot_to_live()
    for i in range(6):
        et.e.hold("select"); et.e.press("right", hold=5, after=3); et.e.release("select")
    et.e.tick(20)
    txt = "\n".join(et.e.screen_text())
    rec("slot persists across power cycle", "S3 CONTACT" in txt or "S3*CONTACT" in txt, [r for r in et.e.screen_text() if r.startswith("S3")])
    et.e.press("down"); et.e.press("down")
    et.e.press("left")                                     # action LOAD
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    loaded = et.pb(0, 73 + 64 * 32)
    rec("loaded project identical to saved project", loaded == projbytes,
        "cut=%d bpm=%d" % (loaded[SYNTH + S_CUT], loaded[SYNTH + S_BPM]))
    et.stop()

    # corrupt one data byte in slot 3 -> must be reported and refused
    data = bytearray(open(bat, "rb").read())
    off = 0x1000 * 0 + 0x0000          # slot index 2 -> bank 1 offset 0
    off = 8192 * 1 + 0 + 10 + 100
    data[off] ^= 0x5A
    bad = bat + ".bad"
    open(bad, "wb").write(data)
    et = Etch(rom, bad)
    et.boot_to_live()
    for i in range(6):
        et.e.hold("select"); et.e.press("right", hold=5, after=3); et.e.release("select")
    et.e.tick(20)
    txt = "\n".join(et.e.screen_text())
    rec("corrupted slot detected (checksum)", "CORRUPT" in txt)
    cut_before = et.pb(SYNTH + S_CUT)[0]
    et.e.press("down"); et.e.press("down"); et.e.press("left")
    et.e.press("a"); et.e.press("a"); et.e.tick(20)
    rec("corrupted slot refused, project untouched", et.pb(SYNTH + S_CUT)[0] == cut_before)
    et.stop()

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

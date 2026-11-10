#!/usr/bin/env python3
"""
ETCH -- A Generative Acid Machine -- by Synth Centric
Record release/ETCH_Demo.wav: a real, unedited performance of the compiled
ROM, captured from the SameBoy APU (DMG model, accurate high-pass) while the
script presses buttons exactly as a player would.  Nothing is synthesised
outside the ROM; the only post-processing is stereo->mono, 48k->32 kHz
resampling and peak normalisation.

usage: record_demo.py ROM OUT.wav
"""
import os
import sys
import tempfile
import wave

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests"))
from sb_util import SB, read_wav  # noqa: E402

SEC = 60  # frames per second (approx. 59.73)


def sel(e, d):
    e.hold("select"); e.press(d, hold=5, after=3); e.release("select")


def load_track(e, t, cur_row):
    """From LIVE: go to SAVE, move to track row, A A, back to LIVE."""
    for _ in range(3):
        sel(e, "right")                 # LIVE -> GEN -> SONG -> SAVE
    e.tick(10)
    target = 8 + t
    while cur_row < target:
        e.press("down", hold=3, after=2); cur_row += 1
    while cur_row > target:
        e.press("up", hold=3, after=2); cur_row -= 1
    e.press("a", hold=3, after=3); e.press("a", hold=3, after=3)
    for _ in range(3):
        sel(e, "left")                  # SAVE -> LIVE
    return cur_row


def sweep(e, button, frames):
    e.hold(button); e.tick(frames); e.release(button); e.tick(2)


def main():
    rom, out = sys.argv[1], sys.argv[2]
    tmp = os.path.join(tempfile.mkdtemp(), "raw.wav")
    e = SB(rom)
    e.tick(240)
    e.press("start")                    # title -> LIVE, track 1 plays
    e.wav_start(tmp)
    # --- T1 FIRST CONTACT: program on LIVE, open the filter, fill, close it
    e.tick(8 * SEC)
    e.press("down", hold=3, after=2)          # BS -> KK row (cursor on step 1)
    for _ in range(2):
        e.press("right", hold=3, after=2)     # step 3
    e.press("a")                              # extra kick
    for _ in range(8):
        e.press("right", hold=3, after=2)     # step 11
    e.press("a")                              # another kick
    e.tick(4 * SEC)
    e.press("up", hold=3, after=2)           # BS row, step 11 (a rest)
    e.press("a", hold=3, after=2)             # add a note (last note: A2)
    e.hold("a")
    for _ in range(3):
        e.press("right", hold=3, after=3)     # A+RIGHT x3 -> C3
    e.release("a")
    e.tick(6 * SEC)
    for _ in range(5):
        e.press("down", hold=3, after=2)    # cursor to CUT
    sweep(e, "right", 90)                    # cutoff up
    e.tick(6 * SEC)
    e.press("down", hold=3, after=2)         # RES
    sweep(e, "right", 30)
    e.tick(4 * SEC)
    e.press("a")                              # FILL
    e.tick(5 * SEC)
    e.press("up", hold=3, after=2)           # CUT
    sweep(e, "left", 90)                      # cutoff back down
    e.tick(4 * SEC)
    # --- T4 CORROSION: CORRODE mode rewrites the line every two bars
    row = load_track(e, 3, 0)
    e.tick(36 * SEC)
    # --- T3 STATIC: dark electro
    row = load_track(e, 2, row)
    e.tick(22 * SEC)
    # --- T6 WAREHOUSE: drive harder, evolve on demand
    row = load_track(e, 5, row)
    e.tick(8 * SEC)
    sel(e, "left"); sel(e, "left")            # LIVE -> TRACK -> SYNTH
    e.hold("b"); e.press("right", hold=3, after=3); e.release("b")   # KCK page
    e.press("down", hold=3, after=2)         # TUNE
    e.hold("a")
    for _ in range(3):
        e.press("left", hold=3, after=3)      # deeper kick
    e.release("a")
    sel(e, "right"); sel(e, "right")          # back to LIVE
    e.tick(4 * SEC)
    for _ in range(5):
        e.press("down", hold=3, after=2)     # CUT -> DRV (LIVE keeps its cursor)
    e.press("right", hold=3, after=2)        # SOFT -> HARD
    e.tick(8 * SEC)
    sel(e, "a")                               # SELECT+A: evolve now
    e.tick(12 * SEC)
    e.wav_stop()
    e.stop()

    d, sr = read_wav(tmp)
    x = d.astype(np.float64).mean(axis=1)
    # resample 48 kHz -> 32 kHz (linear; the GB output is band-limited anyway)
    n = int(len(x) * 32000 / sr)
    xi = np.interp(np.arange(n) * sr / 32000.0, np.arange(len(x)), x)
    xi -= xi.mean()
    xi *= 0.89 * 32767 / max(1.0, np.abs(xi).max())
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(32000)
        w.writeframes(xi.astype("<i2").tobytes())
    print("wrote %s: %.1f s" % (out, n / 32000.0))


if __name__ == "__main__":
    main()

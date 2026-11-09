#!/usr/bin/env python3
"""ETCH -- by Synth Centric.  Flat CPU profile of the ROM in SameBoy:
cycles per function (bank-0 / current-bank addresses mapped via the .noi file).
usage: profile.py ROM [frames] [setup-commands...]"""
import bisect
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from sb_util import SB


def profile(e, frames, top=25):
    tmp = os.path.join(tempfile.mkdtemp(), "prof.txt")
    e.cmd("prof on")
    e.tick(frames)
    e.cmd("prof off")
    e.cmd("profdump " + tmp)
    funcs = sorted((a, n) for n, a in e.syms.items() if a < 0x8000 and not n.startswith("l__") and not n.startswith("s__") and not n.startswith("b_") and not n.startswith("___bank"))
    addrs = [a for a, _ in funcs]
    acc = {}
    total = 0
    for line in open(tmp):
        pc, c = map(int, line.split())
        total += c
        i = bisect.bisect_right(addrs, pc) - 1
        name = funcs[i][1] if i >= 0 else "?"
        acc[name] = acc.get(name, 0) + c
    print("total %d cycles (8MHz units) over %d frames" % (total, frames))
    for n, c in sorted(acc.items(), key=lambda x: -x[1])[:top]:
        print("%6.2f%%  %s" % (100.0 * c / total, n))
    return acc, total


if __name__ == "__main__":
    e = SB(sys.argv[1])
    e.tick(120)
    for c in sys.argv[3:]:
        e.cmd(c)
    profile(e, int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    e.stop()

"""ETCH -- by Synth Centric.  Python driver for the headless SameBoy harness
(tests/sameboy/harness.c).  Mirrors the small API of emu_util.Emu."""
import os
import subprocess
import tempfile
import wave

import numpy as np

from emu_util import load_symbols

HERE = os.path.dirname(os.path.abspath(__file__))
HARNESS = os.path.join(HERE, "sameboy", "bin", "sb_harness")
BOOT = os.path.join(HERE, "sameboy", "bin", "dmg_boot.bin")


def available():
    return os.path.exists(HARNESS) and os.path.exists(BOOT)


class SB:
    def __init__(self, rom, battery=None, model="dmg"):
        self.syms = load_symbols(rom)
        self.tmp = tempfile.mkdtemp()
        args = [HARNESS, BOOT, rom, battery or "-", model]
        self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.frames = 0

    def cmd(self, c):
        self.p.stdin.write(c + "\n")
        self.p.stdin.flush()
        return self.p.stdout.readline().strip()

    def sym(self, name):
        return self.syms["_" + name] if ("_" + name) in self.syms else self.syms[name]

    def peek(self, addr, n=1):
        return bytes.fromhex(self.cmd("peek %x %d" % (addr, n)))

    def rd(self, name, off=0):
        return self.peek(self.sym(name) + off)[0]

    def rd16(self, name, off=0):
        b = self.peek(self.sym(name) + off, 2)
        return b[0] | b[1] << 8

    def wr(self, name, val, off=0):
        self.cmd("poke %x %x" % (self.sym(name) + off, val))

    def tick(self, n=1):
        self.cmd("run %d" % n)
        self.frames += n

    def press(self, *buttons, hold=5, after=4):
        for b in buttons:
            self.cmd("key %s 1" % b)
        self.tick(hold)
        for b in buttons:
            self.cmd("key %s 0" % b)
        self.tick(after)

    def hold(self, *buttons):
        for b in buttons:
            self.cmd("key %s 1" % b)

    def release(self, *buttons):
        for b in buttons:
            self.cmd("key %s 0" % b)

    def cycles(self):
        return int(self.cmd("cycles"))

    def screen_text(self):
        lcdc = self.peek(0xFF40)[0]
        base = 0x9C00 if lcdc & 0x08 else 0x9800
        scx = self.peek(0xFF43)[0] // 8
        scy = self.peek(0xFF42)[0] // 8
        vram = self.peek(base, 1024)
        rows = []
        for y in range(18):
            s = ""
            for x in range(20):
                t = vram[((y + scy) % 32) * 32 + (x + scx) % 32]
                if t < 96:
                    s += chr(t + 32)
                elif t < 160:
                    s += chr(t - 96 + 32)
                else:
                    s += "#"
            rows.append(s)
        return rows

    def screenshot(self, path, scale=3):
        ppm = os.path.join(self.tmp, "s.ppm")
        self.cmd("shot " + ppm)
        from PIL import Image
        img = Image.open(ppm)
        img.resize((160 * scale, 144 * scale), 0).save(path)

    def wav_start(self, path):
        self.cmd("wav " + path)

    def wav_stop(self):
        return self.cmd("wavstop")

    def save_battery(self, path):
        self.cmd("savebat " + path)

    def stop(self):
        try:
            self.cmd("quit")
        except Exception:
            pass
        self.p.wait(timeout=10)


def read_wav(path):
    with wave.open(path) as w:
        d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").reshape(-1, 2)
        return d, w.getframerate()

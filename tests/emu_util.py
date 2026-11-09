"""ETCH -- by Synth Centric.  PyBoy helpers for automated emulator tests."""
import io
import os
import wave
import warnings

warnings.filterwarnings("ignore")
import numpy as np
from pyboy import PyBoy

BUTTONS = ["a", "b", "start", "select", "up", "down", "left", "right"]


def load_symbols(rom_path):
    noi = os.path.splitext(rom_path)[0] + ".noi"
    syms = {}
    if os.path.exists(noi):
        for line in open(noi):
            p = line.split()
            if len(p) == 3 and p[0] == "DEF":
                syms[p[1]] = int(p[2], 16)
    return syms


class Emu:
    def __init__(self, rom, ram_bytes=None, sound=True):
        self.rom = rom
        self.syms = load_symbols(rom)
        if ram_bytes is None:
            ram_bytes = b"\xff" * 32768      # blank, never-written SRAM
        ram = io.BytesIO(ram_bytes)
        self.pb = PyBoy(rom, window="null", sound_emulated=sound, ram_file=ram,
                        cgb=False, log_level="ERROR")
        self.pb.set_emulation_speed(0)
        self.audio = []
        self.record = False
        self.frames = 0

    def sym(self, name):
        return self.syms["_" + name] if ("_" + name) in self.syms else self.syms[name]

    def rd(self, name, off=0):
        return self.pb.memory[self.sym(name) + off]

    def rd16(self, name, off=0):
        a = self.sym(name) + off
        return self.pb.memory[a] | (self.pb.memory[a + 1] << 8)

    def wr(self, name, val, off=0):
        self.pb.memory[self.sym(name) + off] = val

    def tick(self, n=1):
        for _ in range(n):
            self.pb.tick(1, True)
            self.frames += 1
            if self.record:
                self.audio.append(self.pb.sound.ndarray.copy())

    def press(self, *buttons, hold=3, after=4):
        for b in buttons:
            self.pb.button_press(b)
        self.tick(hold)
        for b in buttons:
            self.pb.button_release(b)
        self.tick(after)

    def hold(self, *buttons):
        for b in buttons:
            self.pb.button_press(b)

    def release(self, *buttons):
        for b in buttons:
            self.pb.button_release(b)

    def screen_text(self):
        """Decode the visible background tile map into text (ETCH font map)."""
        tm = self.pb.tilemap_background
        scx = self.pb.memory[0xFF43] // 8
        scy = self.pb.memory[0xFF42] // 8
        rows = []
        for y in range(18):
            s = ""
            for x in range(20):
                t = tm[(x + scx) % 32, (y + scy) % 32]
                if 0 <= t < 96:
                    s += chr(t + 32)
                elif 96 <= t < 160:
                    s += chr(t - 96 + 32)
                else:
                    s += "#"
            rows.append(s)
        return rows

    def screenshot(self, path):
        img = self.pb.screen.image.convert("RGB")
        img = img.resize((img.width * 3, img.height * 3), 0)
        img.save(path)

    def samples(self):
        if not self.audio:
            return np.zeros((0, 2), dtype=np.int8)
        return np.concatenate(self.audio)

    def save_wav(self, path, a=None):
        a = self.samples() if a is None else a
        pcm = (a.astype(np.int16) * 200).clip(-32768, 32767).astype("<i2")
        with wave.open(path, "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(self.pb.sound.sample_rate)
            w.writeframes(pcm.tobytes())

    def cart_ram(self):
        buf = io.BytesIO()
        self.pb.stop(save=True, ram_file=buf) if False else None
        return None

    def stop_get_ram(self):
        buf = io.BytesIO()
        self.pb.stop(ram_file=buf)
        return buf.getvalue()

    def stop(self):
        self.pb.stop(save=False)

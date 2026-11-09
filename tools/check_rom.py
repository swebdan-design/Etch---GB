#!/usr/bin/env python3
"""ETCH -- by Synth Centric.  Validate a Game Boy ROM header (used by make)."""
import sys

NINTENDO_LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D0008111F8889000E"
    "DCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E")
CART = {0x00: "ROM ONLY", 0x19: "MBC5", 0x1A: "MBC5+RAM", 0x1B: "MBC5+RAM+BATTERY"}
RAM = {0: 0, 2: 8, 3: 32, 4: 128, 5: 64}


def check(path, quiet=False):
    rom = open(path, "rb").read()
    errs = []
    if rom[0x104:0x134] != NINTENDO_LOGO:
        errs.append("bad logo")
    hc = 0
    for b in rom[0x134:0x14D]:
        hc = (hc - b - 1) & 0xFF
    if hc != rom[0x14D]:
        errs.append("header checksum %02X != %02X" % (rom[0x14D], hc))
    gc = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    if gc != (rom[0x14E] << 8 | rom[0x14F]):
        errs.append("global checksum mismatch")
    rom_kb = 32 << rom[0x148]
    if rom_kb * 1024 != len(rom):
        errs.append("ROM size byte says %d KB, file is %d bytes" % (rom_kb, len(rom)))
    title = rom[0x134:0x143].split(b"\0")[0].decode("ascii", "replace")
    info = {
        "title": title, "cgb_flag": "%02X" % rom[0x143],
        "cart_type": "%02X %s" % (rom[0x147], CART.get(rom[0x147], "?")),
        "rom_kb": rom_kb, "ram_kb": RAM.get(rom[0x149], "?"),
        "header_checksum": "%02X" % rom[0x14D], "size": len(rom),
    }
    if not quiet:
        print("ROM %s: %s" % (path, info))
    if errs:
        print("ROM CHECK FAILED: " + "; ".join(errs))
        return False, info
    if not quiet:
        print("ROM header OK")
    return True, info


if __name__ == "__main__":
    ok, _ = check(sys.argv[1])
    sys.exit(0 if ok else 1)

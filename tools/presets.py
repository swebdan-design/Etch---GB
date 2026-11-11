#!/usr/bin/env python3
"""
ETCH -- A Generative Acid Machine -- by Synth Centric
The eight ROM tracks ("the album"), written in a compact text notation and
compiled into C data that matches project_t byte-for-byte.

Bass step tokens (16 per pattern, '|' separators ignored):
    C2  F#1  Bb1     note (C1 = lowest; bass lives in octaves 1-3)
    !                accent         ~   slide into the next step
    ?N               probability N/8 (N = 1..8)
    @12 @22 @14 @24 @34 @44 @F @NF   condition (1:2, 2:2, 1:4 ... FILL, !FILL)
    .                rest           =   tie (sustain previous note)
Drum lanes: 16 chars of 'x' (hit) / '.' (rest), order KK SN CH OH.
Kits: KK 0 DEEP 1 PUNCH 2 THUD 3 CLICK | SN 0 SHORT 1 GRIT 2 CLAP 3 RIM |
      CH 0 CLOSED 1 TICK 2 SHAKE 3 SHARP | OH 0 OPEN 1 METAL 2 BLIP 3 CRASH
"""
import os
import sys

NOTE_BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
COND = {"12": 1, "22": 2, "14": 3, "24": 4, "34": 5, "44": 6, "F": 7, "NF": 8}
STYLE = {"ACID": 0, "ELECTRO": 1, "MINIMAL": 2, "BROKEN": 3, "INDUSTRIAL": 4, "SPARSE": 5}
SCALE = {"MINOR": 0, "MAJOR": 1, "DORIAN": 2, "PHRYGIAN": 3, "MINPENT": 4, "CHROMATIC": 5}
MODE = {"STABLE": 0, "DRIFT": 1, "CORRODE": 2}
WAVE = {"SAW": 0, "SQUARE": 1, "HOLLOW": 2, "BITE": 3}
DRIVE = {"OFF": 0, "SOFT": 1, "HARD": 2}
LFO_DEST = {"OFF": 0, "PITCH": 1, "CUT": 2, "RES": 3}
LFO_SHAPE = {"TRI": 0, "SQR": 1, "SAW": 2, "SH": 3}

STEP_NOTE, STEP_TIE, STEP_ACCENT, STEP_SLIDE = 1, 2, 4, 8


def parse_note(tok):
    n = NOTE_BASE[tok[0]]
    i = 1
    if i < len(tok) and tok[i] in "#b":
        n += 1 if tok[i] == "#" else -1
        i += 1
    octave = int(tok[i])
    i += 1
    return (octave - 1) * 12 + n, tok[i:]


def parse_bass(text):
    toks = [t for t in text.replace("|", " ").split() if t]
    assert len(toks) == 16, (len(toks), text)
    notes, flags, prob = [], [], []
    last = 12
    for t in toks:
        p, c, f = 7, 0, 0
        if t == ".":
            notes.append(last); flags.append(0); prob.append(7); continue
        if t == "=":
            notes.append(last); flags.append(STEP_TIE); prob.append(7); continue
        n, mods = parse_note(t)
        assert 0 <= n <= 47, t
        f = STEP_NOTE
        while mods:
            if mods[0] == "!":
                f |= STEP_ACCENT; mods = mods[1:]
            elif mods[0] == "~":
                f |= STEP_SLIDE; mods = mods[1:]
            elif mods[0] == "?":
                p = int(mods[1]) - 1; mods = mods[2:]
            elif mods[0] == "@":
                key = mods[1:3] if mods[1:3] in COND else mods[1:2] if mods[1:2] in COND else None
                if mods[1:3] == "NF":
                    key = "NF"
                assert key, t
                c = COND[key]; mods = mods[1 + len(key):]
            else:
                raise ValueError(t)
        notes.append(n); flags.append(f); prob.append((c << 4) | p); last = n
    return notes, flags, prob


def lane(s):
    s = s.replace("|", "").replace(" ", "")
    assert len(s) == 16, s
    return sum(1 << i for i, ch in enumerate(s) if ch == "x")


def pattern_bytes(pat, style):
    notes, flags, prob = parse_bass(pat["bass"])
    # validate slides (must glide into a note) and ties (must follow sound)
    for i in range(16):
        if flags[i] & STEP_SLIDE:
            assert flags[(i + 1) % 16] & 3 == STEP_NOTE, ("slide into non-note", pat["bass"], i)
        if flags[i] & 3 == STEP_TIE:
            assert flags[(i - 1) % 16] & 3 != 0, ("tie after rest", pat["bass"], i)
    b = bytes(notes) + bytes(flags) + bytes(prob)
    for d in pat["drums"]:
        m = lane(d)
        b += bytes([m & 255, m >> 8])
    b += bytes(pat["kit"])
    b += bytes([0, style, 0, 0])            # lock, style, gen, spare
    assert len(b) == 64
    return b


def track_bytes(t):
    name = t["short"].encode("ascii")[:10].ljust(10, b" ")
    s = t["synth"]
    synth = bytes([WAVE[s["wave"]], s["cut"], s["res"], s["env"], s["dec"], s["acc"],
                   DRIVE[s["drive"]], s["slide"], s.get("lfo_rate", 0), s.get("lfo_depth", 0),
                   LFO_DEST[s.get("lfo_dest", "OFF")], LFO_SHAPE[s.get("lfo_shape", "TRI")],
                   t["bpm"], s.get("swing", 0), 12, s.get("gate", 1)])
    g = t["gen"]
    gen = bytes([g["key"], SCALE[g["scale"]], STYLE[t["style"]], g["density"], g["sync"],
                 g["accent"], g["slide"], g["octave"], g["mutation"], MODE[g["mode"]],
                 g["seed"] & 255, g["seed"] >> 8])
    pats = [c[0] for c in t["chain"]]
    reps = [c[1] for c in t["chain"]]
    assert len(pats) <= 16 and all(1 <= r <= 8 for r in reps)
    chain = bytes(pats + [0xFF] * (16 - len(pats))) + bytes(reps + [1] * (16 - len(reps))) + bytes([1])
    hdr = name + synth + gen + chain + bytes([0, 0])     # mutes, start_pattern
    assert len(hdr) == 73, len(hdr)
    style = STYLE[t["style"]]
    body = b"".join(pattern_bytes(p, style) for p in t["patterns"])
    return hdr + bytes([len(t["patterns"])]) + body


ACID_KIT = [0, 2, 0, 0]

TRACKS = [
    dict(title="CORRODE", short="CORRODE", bpm=124, style="ACID",
         synth=dict(wave="SAW", cut=7, res=5, env=10, dec=7, acc=10, drive="SOFT", slide=5, swing=1),
         gen=dict(key=9, scale="MINOR", density=9, sync=5, accent=8, slide=7, octave=0, mutation=5, mode="DRIFT", seed=0x0F1C),
         patterns=[
             dict(bass="A1! .   A2  A1  | .   A1  C2! A1~ | E2  A1  .   A2~ | G2  A1  .   A1!",
                  drums=["x...x...x...x...", "....x.......x...", "..x...x...x...x.", "................"], kit=ACID_KIT),
             dict(bass="A1! .   A2  A1  | .   A1  C2! A1~ | E2  A1  .   A2~ | G2  E2~ D2  C2!",
                  drums=["x...x...x...x...", "....x.......x...", "..x...x...x...x.", "..............x."], kit=ACID_KIT),
             dict(bass="A1! =   .   .   | .   .   A1  .   | .   .   A1~ C2  | .   .   .   .",
                  drums=["................", "................", "x.x.x.x.x.x.x.x.", "..x...x...x...x."], kit=ACID_KIT),
             dict(bass="A1! A1  A2! A1  | .   A1  C2! A1~ | E2! A1  .   A2~ | G2! A1  E2~ A1!",
                  drums=["x...x...x...x...", "....x.......x..x", "xxxxxxxxxxxxxxxx", "..x...x...x...x."], kit=ACID_KIT),
         ],
         chain=[(0, 4), (1, 4), (0, 4), (2, 2), (3, 8), (1, 4), (2, 2)]),

    dict(title="SLAG", short="SLAG", bpm=130, style="ACID",
         synth=dict(wave="SQUARE", cut=5, res=6, env=12, dec=4, acc=12, drive="SOFT", slide=4),
         gen=dict(key=4, scale="MINOR", density=12, sync=3, accent=9, slide=5, octave=0, mutation=7, mode="DRIFT", seed=0xC0C2),
         patterns=[
             dict(bass="E1! E1  E2  E1  | E1  E1! G1  E1  | E1  E2  E1  E1! | B1~ D2  E1  E1",
                  drums=["x...x...x...x...", "......x.......x.", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[1, 3, 1, 1]),
             dict(bass="E1! E1  E2  E1  | E1  E1! G1  E1  | E1  E2  E1  E1! | G1~ A1  B1! D2",
                  drums=["x...x...x...x...", "......x.......x.", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[1, 3, 1, 1]),
             dict(bass="E1! .   .   E1  | .   .   E1  .   | E1! .   .   E1  | .   E2~ D2  .",
                  drums=["x...x...x...x...", "................", "..x...x...x...x.", "................"], kit=[1, 3, 1, 1]),
         ],
         chain=[(0, 8), (1, 4), (2, 4), (0, 8), (1, 4)]),

    dict(title="303HZ", short="303HZ", bpm=112, style="ELECTRO",
         synth=dict(wave="HOLLOW", cut=4, res=6, env=8, dec=9, acc=8, drive="OFF", slide=8,
                    lfo_rate=4, lfo_depth=5, lfo_dest="CUT", lfo_shape="TRI"),
         gen=dict(key=2, scale="PHRYGIAN", density=6, sync=9, accent=7, slide=6, octave=0, mutation=3, mode="DRIFT", seed=0x5747),
         patterns=[
             dict(bass="D1! .   .   D2  | .   .   F1~ G1  | .   .   D1  .   | Eb1~ D1 .   .",
                  drums=["x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", ".......x......x."], kit=[1, 0, 0, 2]),
             dict(bass="D1! .   .   D2  | .   .   F1~ G1  | .   .   D1  .   | C2! Bb1~ A1 .",
                  drums=["x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", ".......x......x."], kit=[1, 0, 0, 2]),
             dict(bass="G1! .   .   G2  | .   .   Bb1~ C2 | .   .   G1  .   | F1~ Eb1  .   .",
                  drums=["x..x..x...x.....", "....x.......x...", "x.x.x.xxx.x.x.xx", ".......x......x."], kit=[1, 0, 0, 2]),
         ],
         chain=[(0, 4), (1, 4), (0, 4), (2, 4), (1, 4)]),

    dict(title="CAUSTIC", short="CAUSTIC", bpm=135, style="ACID",
         synth=dict(wave="SAW", cut=3, res=7, env=14, dec=5, acc=15, drive="HARD", slide=6),
         gen=dict(key=7, scale="MINOR", density=11, sync=6, accent=12, slide=8, octave=0, mutation=9, mode="CORRODE", seed=0xC0DE),
         patterns=[
             dict(bass="G1! G1  G2~ F2  | G1  .   G1! D2  | G1  G2! .   Bb1~ | C2  G1! G1  F1",
                  drums=["x...x...x...x...", "....x.......x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[3, 1, 3, 1]),
             dict(bass="G1! G1  G2~ F2  | G1  .   G1! D2  | G1  G2! .   Bb1~ | C2! D2~ F2  G2!",
                  drums=["x...x...x...x...", "....x.......x.xx", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[3, 1, 3, 1]),
         ],
         chain=[(0, 8), (1, 4)]),

    dict(title="RUSTROT", short="RUSTROT", bpm=104, style="MINIMAL",
         synth=dict(wave="HOLLOW", cut=9, res=4, env=6, dec=12, acc=6, drive="OFF", slide=11,
                    lfo_rate=3, lfo_depth=7, lfo_dest="CUT", lfo_shape="TRI", gate=2, swing=3),
         gen=dict(key=0, scale="MINOR", density=5, sync=4, accent=5, slide=9, octave=1, mutation=4, mode="DRIFT", seed=0x4E17),
         patterns=[
             dict(bass="C2! =   .   G1~ | Bb1 =   .   .   | C2  .   Eb2~ D2 | C2  =   .   .",
                  drums=["x...x...x...x...", "............x...", "..x...x...x...x.", "..............x."], kit=[0, 3, 1, 0]),
             dict(bass="C2! =   .   G1~ | Bb1 =   .   .   | Ab1 .   G1~ F1  | G1  =   .   .",
                  drums=["x...x...x...x...", "............x...", "..x...x...x...x.", "..............x."], kit=[0, 3, 1, 0]),
         ],
         chain=[(0, 4), (1, 4)]),

    dict(title="VOLTAGE", short="VOLTAGE", bpm=128, style="ACID",
         synth=dict(wave="SQUARE", cut=6, res=5, env=11, dec=6, acc=11, drive="SOFT", slide=5),
         gen=dict(key=5, scale="MINOR", density=12, sync=5, accent=9, slide=6, octave=0, mutation=6, mode="DRIFT", seed=0x3A7E),
         patterns=[
             dict(bass="F1! F1  F2  F1  | Ab1~ F1 F1! C2  | F1  F1  F2~ Eb2 | F1! C2  F1  Ab1",
                  drums=["x...x...x...x...", "....x.......x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[0, 2, 0, 0]),
             dict(bass="F1! F1  F2  F1  | Ab1~ F1 F1! C2  | F1  F1  F2~ Eb2 | Db2! C2~ Ab1 F1",
                  drums=["x...x...x...x...", "....x.......x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x."], kit=[0, 2, 0, 0]),
             dict(bass="F1! .   .   .   | .   .   .   .   | F1  .   F2~ Eb2 | .   .   .   .",
                  drums=["x...............", "................", "..x...x...x...x.", "................"], kit=[0, 2, 0, 0]),
         ],
         chain=[(0, 4), (1, 4), (0, 4), (2, 2), (1, 4), (0, 4)]),

    dict(title="HEXDRUM", short="HEXDRUM", bpm=118, style="BROKEN",
         synth=dict(wave="BITE", cut=8, res=6, env=9, dec=5, acc=10, drive="HARD", slide=3,
                    lfo_rate=9, lfo_depth=10, lfo_dest="RES", lfo_shape="SH"),
         gen=dict(key=11, scale="PHRYGIAN", density=7, sync=12, accent=9, slide=4, octave=0, mutation=6, mode="CORRODE", seed=0x5105),
         patterns=[
             dict(bass="B1! .   C2?5 .  | B1  F#2~@12 E2 . | B1! .   .   D2?4 | C2~ B1  .   A1@34",
                  drums=["x......x..x.....", "....x....x..x...", ".x.x..x..x.x..x.", "..............x."], kit=[1, 1, 2, 3]),
             dict(bass="B1! .   C2  B1@22 | .  F#2~ G2  .   | B1! A1?6 .  D2  | .   C2! B1  .",
                  drums=["x......x..x..x..", "....x....x..x..x", ".x.x..x..x.x..x.", "..............x."], kit=[1, 1, 2, 3]),
         ],
         chain=[(0, 4), (1, 4)]),

    dict(title="ACIDBURN", short="ACIDBURN", bpm=120, style="MINIMAL",
         synth=dict(wave="HOLLOW", cut=11, res=3, env=5, dec=10, acc=5, drive="OFF", slide=9,
                    lfo_rate=2, lfo_depth=4, lfo_dest="CUT", lfo_shape="TRI", gate=2),
         gen=dict(key=9, scale="DORIAN", density=4, sync=6, accent=4, slide=8, octave=0, mutation=2, mode="DRIFT", seed=0xAF7E),
         patterns=[
             dict(bass="A1  .   .   A1  | .   .   E2  .   | A1  .   .   A1  | .   .   D2~ E2",
                  drums=["x...x...x...x...", "..............x.", "..x...x...x...x.", "................"], kit=[0, 3, 1, 1]),
             dict(bass="A1  .   .   A1  | .   .   E2  .   | A1  .   .   A1  | .   F#2~ E2 .",
                  drums=["x...x...x...x...", "..............x.", "..x...x...x...x.", "......x........."], kit=[0, 3, 1, 1]),
         ],
         chain=[(0, 8), (1, 4)]),
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "src/presets/preset_data.c"
    names_out = os.path.join(os.path.dirname(out), "track_names.c")
    assert len(TRACKS) == 8
    L = ["/* AUTOGENERATED by tools/presets.py -- do not edit. */",
         "/* ETCH -- A Generative Acid Machine -- by Synth Centric: the ROM tracks */",
         "#pragma bank 255",
         "#include <gbdk/platform.h>",
         "#include <stddef.h>",
         "#include \"engine.h\"",
         "#include \"presets.h\"",
         "BANKREF(preset_data)",
         "/* the header bytes must match project_t up to .pat */",
         "typedef char preset_hdr_check[(offsetof(project_t, pat) == PRESET_HDR) ? 1 : -1];",
         "typedef char pattern_size_check[(sizeof(pattern_t) == 64) ? 1 : -1];"]
    for i, t in enumerate(TRACKS):
        b = track_bytes(t)
        L.append("static const uint8_t trk%d[%d] = {" % (i, len(b)))
        for j in range(0, len(b), 16):
            L.append("  " + ",".join("0x%02X" % x for x in b[j:j + 16]) + ",")
        L.append("};")
    L.append("static const uint8_t * const tracks[NUM_TRACKS] = { %s };" %
             ", ".join("trk%d" % i for i in range(8)))
    L.append("""
void preset_load(uint8_t t) BANKED
{
    const uint8_t *d = tracks[t & 7];
    uint8_t i, n;
    memcpy(&proj, d, PRESET_HDR);
    n = d[PRESET_HDR];
    d += PRESET_HDR + 1;
    for (i = 0; i < NUM_PATTERNS; i++) {
        if (i < n) memcpy(&proj.pat[i], d + (uint16_t)i * 64, 64);
        else pattern_clear(&proj.pat[i]);
    }
    project_drum_defaults(&proj);
}
""")
    open(out, "w").write("\n".join(L) + "\n")
    N = ["/* AUTOGENERATED by tools/presets.py -- do not edit. */",
         "#include \"presets.h\"",
         "const char * const track_names[NUM_TRACKS] = {"]
    N += ["    \"%s\"," % t["title"] for t in TRACKS]
    N.append("};")
    open(names_out, "w").write("\n".join(N) + "\n")
    print("8 tracks, %d bytes" % sum(len(track_bytes(t)) for t in TRACKS))


if __name__ == "__main__":
    main()

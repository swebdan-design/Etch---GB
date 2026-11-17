#!/usr/bin/env python3
"""
ETCH -- A Generative Acid Machine -- by Synth Centric
Graphics generator, in the series look (SERIES_SPEC.md 4): ETCH DS's 5x7
MAIN font (thin, all capitals), grey captions, rounded step cells, hairline
sliders, the DS-style title, and the Game Boy Color palettes that show the
series accent #F45423.
Emits src/ui/gfx_data.c (autobanked) and include/gfx_tiles.h.

Tile map in VRAM (BG, unsigned $8000 addressing):
   0..63    ASCII 32..95, black ink on white paper (values)
  64..95    caption letters CAP_CHARS, dark grey ink (captions)
  96..159   ASCII 32..95 inverted: the cursor / lit (black, or the accent on GBC)
 160..223   UI symbol tiles (below)
 224..255   waveform scope tiles (written at run time)
 the title screen temporarily overwrites 160..255 with its own tiles.
"""
import os
import sys

# --- 5x7 glyphs ('#' = ink) -------------------------------------------------
G = {}
def g(ch, *rows):
    assert len(rows) == 7, ch
    for r in rows:
        assert len(r) == 5, (ch, r)
    G[ch] = rows

g(' ', ".....", ".....", ".....", ".....", ".....", ".....", ".....")
g('!', "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#..")
g('"', ".#.#.", ".#.#.", ".....", ".....", ".....", ".....", ".....")
g('#', ".#.#.", "#####", ".#.#.", ".#.#.", ".#.#.", "#####", ".#.#.")
g('$', "..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#..")
g('%', "##..#", "##.#.", "...#.", "..#..", ".#...", ".#.##", "#..##")
g('&', ".##..", "#..#.", ".##..", ".#...", "#.#.#", "#..#.", ".##.#")
g("'", "..#..", "..#..", ".....", ".....", ".....", ".....", ".....")
g('(', "...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#.")
g(')', ".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#...")
g('*', ".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", ".....")
g('+', ".....", "..#..", "..#..", "#####", "..#..", "..#..", ".....")
g(',', ".....", ".....", ".....", ".....", "..#..", "..#..", ".#...")
g('-', ".....", ".....", ".....", "####.", ".....", ".....", ".....")
g('.', ".....", ".....", ".....", ".....", ".....", "..#..", "..#..")
g('/', "....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#....")
g('0', ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.")
g('1', "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.")
g('2', ".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####")
g('3', "####.", "....#", "....#", ".###.", "....#", "....#", "####.")
g('4', "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.")
g('5', "#####", "#....", "####.", "....#", "....#", "#...#", ".###.")
g('6', ".###.", "#....", "#....", "####.", "#...#", "#...#", ".###.")
g('7', "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...")
g('8', ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.")
g('9', ".###.", "#...#", "#...#", ".####", "....#", "....#", ".###.")
g(':', ".....", "..#..", "..#..", ".....", "..#..", "..#..", ".....")
g(';', ".....", "..#..", "..#..", ".....", "..#..", "..#..", ".#...")
g('<', "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#.")
g('=', ".....", ".....", "####.", ".....", "####.", ".....", ".....")
g('>', ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#...")
g('?', ".###.", "#...#", "....#", "..##.", "..#..", ".....", "..#..")
g('@', ".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###.")
g('A', ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#")
g('B', "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####.")
g('C', ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.")
g('D', "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####.")
g('E', "#####", "#....", "#....", "####.", "#....", "#....", "#####")
g('F', "#####", "#....", "#....", "####.", "#....", "#....", "#....")
g('G', ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####")
g('H', "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#")
g('I', ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.")
g('J', "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##..")
g('K', "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#")
g('L', "#....", "#....", "#....", "#....", "#....", "#....", "#####")
g('M', "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#")
g('N', "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#")
g('O', ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.")
g('P', "####.", "#...#", "#...#", "####.", "#....", "#....", "#....")
g('Q', ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#")
g('R', "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#")
g('S', ".####", "#....", "#....", ".###.", "....#", "....#", "####.")
g('T', "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..")
g('U', "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.")
g('V', "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#..")
g('W', "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#")
g('X', "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#")
g('Y', "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..")
g('Z', "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####")
g('[', ".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###.")
g('\\', "#....", ".#...", ".#...", "..#..", "...#.", "...#.", "....#")
g(']', ".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###.")
g('^', "..#..", ".#.#.", "#...#", ".....", ".....", ".....", ".....")
g('_', ".....", ".....", ".....", ".....", ".....", ".....", "#####")
g('`', ".#...", "..#..", ".....", ".....", ".....", ".....", ".....")
g('a', ".....", ".....", ".###.", "....#", ".####", "#...#", ".####")
g('b', "#....", "#....", "####.", "#...#", "#...#", "#...#", "####.")
g('c', ".....", ".....", ".###.", "#....", "#....", "#....", ".###.")
g('d', "....#", "....#", ".####", "#...#", "#...#", "#...#", ".####")
g('e', ".....", ".....", ".###.", "#...#", "#####", "#....", ".###.")
g('f', "..##.", ".#...", "####.", ".#...", ".#...", ".#...", ".#...")
g('g', ".....", ".####", "#...#", "#...#", ".####", "....#", ".###.")
g('h', "#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#")
g('i', "..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###.")
g('j', "...#.", ".....", "..##.", "...#.", "...#.", "#..#.", ".##..")
g('k', "#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#.")
g('l', ".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.")
g('m', ".....", ".....", "##.#.", "#.#.#", "#.#.#", "#.#.#", "#...#")
g('n', ".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#")
g('o', ".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###.")
g('p', ".....", "####.", "#...#", "#...#", "####.", "#....", "#....")
g('q', ".....", ".####", "#...#", "#...#", ".####", "....#", "....#")
g('r', ".....", ".....", "#.##.", "##..#", "#....", "#....", "#....")
g('s', ".....", ".....", ".####", "#....", ".###.", "....#", "####.")
g('t', ".#...", ".#...", "####.", ".#...", ".#...", ".#..#", "..##.")
g('u', ".....", ".....", "#...#", "#...#", "#...#", "#..##", ".##.#")
g('v', ".....", ".....", "#...#", "#...#", "#...#", ".#.#.", "..#..")
g('w', ".....", ".....", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#.")
g('x', ".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#")
g('y', ".....", "#...#", "#...#", "#...#", ".####", "....#", ".###.")
g('z', ".....", ".....", "#####", "...#.", "..#..", ".#...", "#####")
g('{', "...##", "..#..", "..#..", ".#...", "..#..", "..#..", "...##")
g('|', "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..")
g('}', "##...", "..#..", "..#..", "...#.", "..#..", "..#..", "##...")
g('~', ".....", ".....", ".#...", "#.#.#", "...#.", ".....", ".....")
g('\x7f', "#####", "#####", "#####", "#####", "#####", "#####", "#####")


import series_fonts as SF

CAP_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"     # caption glyphs: tiles 64..89 (90..95 spare)
assert len(CAP_CHARS) <= 32


def main_rows(ch):
    """the series MAIN glyph for ch (capitals; ETCH's own glyph if the series font lacks it)"""
    ch = ch.upper() if 'a' <= ch <= 'z' else ch
    if ch in SF.MAIN:
        return SF.MAIN[ch]
    return G[ch] if ch in G else SF.MAIN['?']


def glyph_pixels(ch, ink=3):
    """8x8 pixel matrix (values 0..3): the 5x7 glyph, centred in the cell, thin"""
    rows = main_rows(ch)
    w = len(rows[0])
    ox = 1 + (5 - w) // 2
    px = [[0] * 8 for _ in range(8)]
    for y in range(7):
        for x in range(w):
            if rows[y][x] == '#':
                px[y][ox + x] = ink
    return px


def tile_bytes(px):
    """8x8 matrix of colour indices -> 16 bytes 2bpp."""
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            c = px[y][x]
            lo |= (c & 1) << (7 - x)
            hi |= ((c >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out


def invert(px):
    return [[3 - c for c in row] for row in px]


def from_art(rows, palette=None):
    """rows: 8 strings of 8 chars using '.', '-', '+', '#' for shades 0..3."""
    m = {'.': 0, '-': 1, '+': 2, '#': 3}
    assert len(rows) == 8
    return [[m[c] for c in r] for r in rows]


# --- UI symbol tiles (index 160+) -------------------------------------------
SYM = []      # (name, pixels)
def sym(name, *rows):
    SYM.append((name, from_art(rows)))

# sliders (SERIES_SPEC.md 4.5): a 1 px light track, a 3 px dark fill, and a
# 2 px black thumb at the end of the fill.  METER0..7 hold the thumb (the tile
# where the fill ends), METER8 is a full tile, METER_E an empty track.
for n in range(9):
    rows = []
    for y in range(8):
        r = ""
        for x in range(8):
            if n < 8 and x in (n, n + 1) and 1 <= y <= 6:
                r += '#'
            elif x < n and 3 <= y <= 5:
                r += '+'
            elif y == 4:
                r += '-'
            else:
                r += '.'
        rows.append(r)
    sym("METER%d" % n, *rows)
sym("METER_E", "........", "........", "........", "........", "--------", "........", "........", "........")

# step cells: rounded 6x6, off = light, note/hit = dark, accent/kick = black
sym("CELL_OFF",   "........", "..----..", ".------.", ".------.", ".------.", ".------.", "..----..", "........")
sym("CELL_BEAT",  "........", "..----..", ".------.", ".------.", ".------.", ".------.", "..----..", "...++...")
sym("CELL_ON",    "........", "..++++..", ".++++++.", ".++++++.", ".++++++.", ".++++++.", "..++++..", "........")
sym("CELL_ACC",   "........", "..####..", ".######.", ".######.", ".######.", ".######.", "..####..", "........")
sym("CELL_SLIDE", "........", "..++++..", ".++++++.", ".++++++.", ".+++++++", ".+++++++", "..++++..", "........")
sym("CELL_TIE",   "........", "........", "........", "++++++++", "++++++++", "........", "........", "........")
sym("CELL_CUR_OFF", "########", "#.----.#", "#------#", "#------#", "#------#", "#------#", "#.----.#", "########")
sym("CELL_CUR_ON",  "########", "#.++++.#", "#++++++#", "#++++++#", "#++++++#", "#++++++#", "#.++++.#", "########")
sym("CELL_MUTED", "........", "..----..", ".------.", ".--++--.", ".--++--.", ".------.", "..----..", "........")
sym("PLAYHEAD",   "........", "........", "........", ".######.", "..####..", "...##...", "........", "........")
sym("PLAY",       "........", ".##.....", ".####...", ".######.", ".######.", ".####...", ".##.....", "........")
sym("STOP",       "........", "........", ".######.", ".######.", ".######.", ".######.", "........", "........")
sym("LOCK",       "..###...", ".#...#..", ".#...#..", "#######.", "###.###.", "###.###.", "#######.", "........")
sym("MUTE",       "#......#", ".#....#.", "..#..#..", "...##...", "...##...", "..#..#..", ".#....#.", "#......#")
sym("ACCENT",     "...#....", "..###...", ".#####..", "#######.", "...#....", "...#....", "...#....", "........")
sym("SLIDE",      "........", "......##", "....##..", "..##....", "##......", "........", "########", "........")
sym("ARROW_R",    "........", "...+....", "....+...", ".++++++.", "....+...", "...+....", "........", "........")
sym("RULE",       "........", "........", "........", "........", "--------", "........", "........", "........")
sym("RULE_HEAVY", "........", "........", "........", "--------", "........", "........", "........", "........")
sym("DOT",        "........", "........", "........", "........", "...++...", "........", "........", "........")
sym("BLOCK",      "########", "########", "########", "########", "########", "########", "########", "########")
sym("HATCH",      "#.#.#.#.", ".#.#.#.#", "#.#.#.#.", ".#.#.#.#", "#.#.#.#.", ".#.#.#.#", "#.#.#.#.", ".#.#.#.#")
sym("GREY",       "--------", "--------", "--------", "--------", "--------", "--------", "--------", "--------")
sym("VBAR",       "...##...", "...##...", "...##...", "...##...", "...##...", "...##...", "...##...", "...##...")
sym("EVO",        "........", ".#...#..", "..#.#...", "...#....", "..#.#...", ".#...#..", "#######.", "........")
sym("FILLMARK",   "#.#.#.#.", "########", "........", "........", "........", "........", "........", "........")
sym("CORNER_TL",  "########", "#-------", "#-......", "#-......", "#-......", "#-......", "#-......", "#-......")
sym("CORNER_TR",  "########", "-------#", "......-#", "......-#", "......-#", "......-#", "......-#", "......-#")
sym("ACT0",       "........", "........", "........", "........", "........", "........", "........", "########")
sym("ACT1",       "........", "........", "........", "........", "........", "........", "########", "########")
sym("ACT2",       "........", "........", "........", "........", "########", "########", "########", "########")
sym("ACT3",       "........", "........", "########", "########", "########", "########", "########", "########")
sym("ACT4",       "########", "########", "########", "########", "########", "########", "########", "########")
sym("ARROW_L",    "........", "....#...", "...##...", "..#####.", "...##...", "....#...", "........", "........")
sym("HIT",        "........", "........", "..####..", "..####..", "..####..", "..####..", "........", "........")
sym("TIE_DASH",   "........", "........", "........", "########", "........", "........", "........", "........")
assert len(SYM) <= 64

# --- title (SERIES_SPEC.md 3.3, laid out as ETCH DS's) ------------------------
RELEASE = "3.0"          # keep in step with ETCH_RELEASE_STR in include/ui.h


def text_px(text, font, ink, scale=1):
    """a line of series-font text as a pixel matrix (h x w)"""
    f = SF.MAIN if font == "main" else SF.SMALL
    h = 7 if font == "main" else 5
    glyphs = [f.get(c, f['?']) for c in text]
    w = sum(len(g[0]) + 1 for g in glyphs) - 1
    px = [[0] * (w * scale) for _ in range(h * scale)]
    x = 0
    for g in glyphs:
        for y in range(h):
            for i, ch in enumerate(g[y]):
                if ch == '#':
                    for dy in range(scale):
                        for dx in range(scale):
                            px[y * scale + dy][(x + i) * scale + dx] = ink
        x += len(g[0]) + 1
    return px


def centred_band(lines, height_tiles):
    """place lines [(px, y)] centred on a 160 x (8*height_tiles) band"""
    W, H = 160, height_tiles * 8
    band = [[0] * W for _ in range(H)]
    for px, oy in lines:
        ox = (W - len(px[0])) // 2
        for y, row in enumerate(px):
            for x, v in enumerate(row):
                if v:
                    band[oy + y][ox + x] = v
    return band


def title_bands():
    """the wordmark band (rows 2..5) and the small-type lines (one row each)"""
    mark = text_px("ETCH", "main", 3, 4)                     # 4x: 20 x 28 px glyphs
    logo = centred_band([(mark, 2)], 4)
    sub = centred_band([(text_px("A GENERATIVE ACID MACHINE", "small", 2), 1)], 1)
    ver = centred_band([(text_px("VERSION " + RELEASE + "   CORROSION FROM ETCH 2.5", "small", 2), 1)], 1)
    hp = centred_band([(text_px("HEADPHONES RECOMMENDED", "small", 1), 1)], 1)
    rule = centred_band([([[1] * 24], 3)], 1)
    return logo, [sub, rule, ver, hp]


def cut_band(px, tiles, index):
    """cut a band into tiles (deduplicated into `tiles`), return its map rows"""
    rows = []
    for ty in range(len(px) // 8):
        row = []
        for tx in range(len(px[0]) // 8):
            t = tuple(tile_bytes([[px[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)]))
            if t not in index:
                index[t] = len(tiles)
                tiles.append(t)
            row.append(160 + index[t])
        rows.append(row)
    return rows


# --- Game Boy Color: palettes and the palette of every tile (SERIES_SPEC.md 4.1) ---
def rgb15(h):
    r, g, b = int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16)
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


WHITE, LIGHT, DARK, BLACK, ACCENT = "#FFFFFF", "#BDBDBD", "#6B6B6B", "#101010", "#F45423"
CGB_PALS = [
    [WHITE, LIGHT, DARK, BLACK],     # 0 grey: everything
    [BLACK, LIGHT, DARK, ACCENT],    # 1 lit: the inverted tiles become black ink on accent paper
    [WHITE, LIGHT, DARK, ACCENT],    # 2 accent: the playhead, the cell cursor, the play mark
]
ACCENT_SYMS = ("PLAYHEAD", "CELL_CUR_OFF", "CELL_CUR_ON", "PLAY", "ARROW_L", "FILLMARK")


def carr(name, data, per=16):
    s = "const uint8_t %s[%d] = {\n" % (name, len(data))
    for i in range(0, len(data), per):
        s += "  " + ",".join("0x%02X" % b for b in data[i:i + per]) + ",\n"
    return s + "};\n"


def main():
    src_out = sys.argv[1] if len(sys.argv) > 1 else "src/ui/gfx_data.c"
    hdr_out = sys.argv[2] if len(sys.argv) > 2 else "include/gfx_tiles.h"
    font = []
    for code in range(32, 96):
        font += tile_bytes(glyph_pixels(chr(code)))
    for ch in CAP_CHARS:                                   # captions: dark grey ink
        font += tile_bytes(glyph_pixels(ch, ink=2))
    font += [0] * 16 * (32 - len(CAP_CHARS))
    inv = []
    for code in range(32, 96):
        inv += tile_bytes(invert(glyph_pixels(chr(code))))
    syms = []
    for _, p in SYM:
        syms += tile_bytes(p)
    # title: the wordmark and the small-type lines share one deduplicated set
    ltiles, index = [], {}
    logo_px, lines = title_bands()
    lmap = cut_band(logo_px, ltiles, index)
    line_maps = [cut_band(b, ltiles, index)[0] for b in lines]
    assert len(ltiles) <= 96, len(ltiles)                  # 160..255 while the title shows
    logo = [b for t in ltiles for b in t]
    lmap_flat = [i for row in lmap for i in row]
    attr = [0] * 256
    for i in range(96, 160):
        attr[i] = 1
    for i, (name, _) in enumerate(SYM):
        if name in ACCENT_SYMS:
            attr[160 + i] = 2
    pals = [rgb15(c) for pal in CGB_PALS for c in pal]

    with open(src_out, "w") as f:
        f.write("/* AUTOGENERATED by tools/gen_gfx.py -- do not edit. */\n")
        f.write("/* ETCH -- A Generative Acid Machine -- by Synth Centric */\n")
        f.write("#pragma bank 255\n#include <gbdk/platform.h>\n#include <stdint.h>\n")
        f.write("#include \"gfx_tiles.h\"\nBANKREF(gfx_data)\n")
        f.write(carr("gfx_font", font))
        f.write(carr("gfx_font_inv", inv))
        f.write(carr("gfx_sym", syms))
        f.write(carr("gfx_logo_tiles", logo))
        f.write(carr("gfx_logo_map", lmap_flat, per=20))
        for i, m in enumerate(line_maps):
            f.write(carr("gfx_title_line%d" % i, m, per=20))
        f.write("""
/* Loader lives in the same bank as the data. */
void gfx_load_ui_tiles(void) BANKED {
    set_bkg_data(0, 96, gfx_font);
    set_bkg_data(96, 64, gfx_font_inv);
    set_bkg_data(160, GFX_SYM_COUNT, gfx_sym);
}
void gfx_load_logo(void) BANKED {
    set_bkg_data(160, GFX_LOGO_TILES, gfx_logo_tiles);
}
void gfx_draw_logo(uint8_t x, uint8_t y) BANKED {
    set_bkg_tiles(x, y, GFX_LOGO_W, GFX_LOGO_H, gfx_logo_map);
}
static const uint16_t gfx_cgb_pals[12] = { %s };
/* Game Boy Color: the series palettes (SERIES_SPEC.md 4.1) */
void gfx_cgb_load(void) BANKED {
    set_bkg_palette(0, 3, gfx_cgb_pals);
}
/* the title's small-type lines: 0 subtitle, 1 rule, 2 version, 3 headphones */
void gfx_draw_title_line(uint8_t n, uint8_t y) BANKED {
    static const uint8_t * const lines[4] = { gfx_title_line0, gfx_title_line1, gfx_title_line2, gfx_title_line3 };
    set_bkg_tiles(0, y, 20, 1, lines[n & 3]);
}
""" % ", ".join("0x%04X" % v for v in pals))
    with open(hdr_out, "w") as f:
        f.write("/* AUTOGENERATED by tools/gen_gfx.py -- do not edit. */\n")
        f.write("#ifndef GFX_TILES_H\n#define GFX_TILES_H\n#include <gbdk/platform.h>\n")
        f.write("#define T_CAP_BASE 64\n")
        f.write("#define CAP_CHARS \"%s\"\n" % CAP_CHARS)
        f.write("#define T_INV_OFFSET 64\n")
        f.write("#define T_SYM_BASE 160\n")
        for i, (name, _) in enumerate(SYM):
            f.write("#define T_%s %d\n" % (name, 160 + i))
        f.write("#define GFX_SYM_COUNT %d\n" % len(SYM))
        f.write("#define GFX_LOGO_TILES %d\n" % len(ltiles))
        f.write("#define GFX_LOGO_W 20\n#define GFX_LOGO_H %d\n" % len(lmap))
        f.write("#define T_SCOPE_BASE 224\n")
        f.write("BANKREF_EXTERN(gfx_data)\n")
        f.write("void gfx_load_ui_tiles(void) BANKED;\n")
        f.write("void gfx_load_logo(void) BANKED;\n")
        f.write("void gfx_draw_logo(uint8_t x, uint8_t y) BANKED;\n")
        f.write("void gfx_draw_title_line(uint8_t n, uint8_t y) BANKED;\n")
        f.write("/* Game Boy Color (SERIES_SPEC.md 4.1): 3 palettes x 4 colours, and each tile's palette */\n")
        f.write("void gfx_cgb_load(void) BANKED;\n")
        accent = [160 + i for i, (n, _) in enumerate(SYM) if n in ACCENT_SYMS]
        f.write("/* tiles shown in palette 2 (accent): %s */\n" % " ".join(n for n, _ in SYM if n in ACCENT_SYMS))
        f.write("#define GFX_CGB_ACCENT(t) (%s)\n" % " || ".join("(t) == %d" % t for t in accent))
        f.write("#endif\n")
    print("font 64 + %d captions, %d symbols, title %d tiles" % (len(CAP_CHARS), len(SYM), len(ltiles)))


if __name__ == "__main__":
    main()

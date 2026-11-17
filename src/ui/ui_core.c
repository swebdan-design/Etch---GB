/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * UI core: shadow tile map with per-row dirty tracking, text/meter helpers,
 * header + message line, input with D-pad auto-repeat.  Bank 0 so that any
 * banked screen can pass pointers to its own string constants.
 */
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

static uint8_t map[SCR_H][SCR_W];
static uint8_t dirty[SCR_H];
static const uint16_t rowoff[SCR_H] = {
    0, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320, 340
};
#define rowp(y) (&map[0][0] + rowoff[y])

/* decimal digits without division (SDCC's 16-bit modulo is very slow) */
static uint8_t digs[5];
static void to_dec(uint16_t v)
{
    static const uint16_t pw[4] = { 10000, 1000, 100, 10 };
    uint8_t i, d;
    for (i = 0; i < 4; i++) {
        d = 0;
        while (v >= pw[i]) { v -= pw[i]; d++; }
        digs[i] = d;
    }
    digs[4] = (uint8_t)v;
}

extern uint16_t vr_src, vr_dst;
extern uint8_t vr_n;
void vram_copy_asm(void);

uint8_t key_held, key_pressed, key_repeat, key_released;
uint16_t ui_entropy = 0xE7C4;
static uint8_t key_prev, rep_timer, a_used, b_used;

const char * const scr_names[SCR_COUNT] = { "LIVE ", "TRACK", "SYNTH", "GEN  ", "SONG ", "FILE " };
extern uint8_t app_screen;

static char msg_buf[SCR_W + 1];
static uint8_t msg_timer;
static const char *hdr_name;
static char row1_buf[SCR_W + 1];
static uint8_t row1_on;
static uint8_t hc_cur = 0xFF, hc_q, hc_pos, hc_bpm, hc_song;

static const char note_names[] = "C-C#D-D#E-F-F#G-G#A-A#B-";

void ui_cls(void)
{
    uint8_t y, x;
    for (y = 0; y < SCR_H; y++) {
        for (x = 0; x < SCR_W; x++) map[y][x] = 0;
        dirty[y] = 1;
    }
}

void ui_put(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t *p;
    if (x >= SCR_W || y >= SCR_H) return;
    p = rowp(y) + x;
    if (*p != t) {
        *p = t;
        dirty[y] = 1;
    }
}

uint8_t ui_get(uint8_t x, uint8_t y)
{
    return rowp(y)[x];
}

void ui_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t t)
{
    while (w--) ui_put(x++, y, t);
}

/* the series is all capitals (SERIES_SPEC.md 4.3): lower case shows as upper */
static uint8_t tile_of(uint8_t c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    return c >= 32 && c < 96 ? (uint8_t)(c - 32) : 0;
}

void ui_text(uint8_t x, uint8_t y, const char *s)
{
    while (*s && x < SCR_W) ui_put(x++, y, tile_of((uint8_t)*s++));
}

void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n)
{
    while (n--) {
        uint8_t c = (uint8_t)*s;
        if (c) s++;
        ui_put(x++, y, tile_of(c));
    }
}

/* captions: letters in dark grey (tiles 64..89), anything else as normal text;
 * n = width to pad/clip to (0: the string's length) */
void ui_capn(uint8_t x, uint8_t y, const char *s, uint8_t n)
{
    uint8_t c, pad = n != 0;
    if (!pad) n = SCR_W;
    while (n-- && x < SCR_W) {
        c = (uint8_t)*s;
        if (c) s++;
        else if (!pad) break;                /* n = 0: just the string */
        if (c >= 'a' && c <= 'z') c -= 32;
        ui_put(x++, y, c >= 'A' && c <= 'Z' ? (uint8_t)(T_CAP_BASE - 'A' + c) : tile_of(c));
    }
}

/* dim: the letters already in a run of cells become captions (dark grey) */
void ui_dim(uint8_t x, uint8_t y, uint8_t w)
{
    while (w-- && x < SCR_W) {
        uint8_t t = map[y][x];
        if (t >= 'A' - 32 && t <= 'Z' - 32) ui_put(x, y, (uint8_t)(t + T_CAP_BASE - ('A' - 32)));
        x++;
    }
}

/* the cursor / lit: inverted text (black on DMG, the accent on GBC) */
void ui_invert(uint8_t x, uint8_t y, uint8_t w)
{
    while (w--) {
        uint8_t t = map[y][x];
        if (t < 64) ui_put(x, y, (uint8_t)(t + 96));
        else if (t < 90) ui_put(x, y, (uint8_t)(t + 65));      /* caption letter -> lit letter */
        else if (t >= 96 && t < 160) ui_put(x, y, (uint8_t)(t - 96));
        x++;
    }
}

void ui_num(uint8_t x, uint8_t y, uint16_t v, uint8_t digits)
{
    uint8_t i, lead = 1;
    to_dec(v);
    for (i = (uint8_t)(5 - digits); i < 5; i++) {
        if (digs[i] || i == 4) lead = 0;
        ui_put(x++, y, lead ? 0 : (uint8_t)('0' - 32 + digs[i]));
    }
}

void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits)
{
    uint8_t i;
    to_dec(v);
    for (i = (uint8_t)(5 - digits); i < 5; i++) ui_put(x++, y, (uint8_t)('0' - 32 + digs[i]));
}

void ui_hex(uint8_t x, uint8_t y, uint16_t v, uint8_t digits)
{
    uint8_t i = digits;
    while (i--) {
        uint8_t d = (uint8_t)(v & 15);
        ui_put((uint8_t)(x + i), y, (uint8_t)((d < 10 ? '0' + d : 'A' + d - 10) - 32));
        v >>= 4;
    }
}

void ui_note(uint8_t x, uint8_t y, uint8_t note)
{
    uint8_t oct = 0;
    while (note >= 12) { note -= 12; oct++; }
    ui_put(x, y, (uint8_t)(note_names[note * 2] - 32));
    ui_put((uint8_t)(x + 1), y, (uint8_t)(note_names[note * 2 + 1] - 32));
    ui_put((uint8_t)(x + 2), y, (uint8_t)('1' - 32 + oct));
}

void ui_meter(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max)
{
    /* w tiles, 8 px each */
    uint16_t num = (uint16_t)v * (uint16_t)(w * 8) + (max >> 1);
    uint8_t px = 0;
    if (max) while (num >= max) { num -= max; px++; }
    /* full tiles, then the tile holding the thumb, then empty track */
    while (w--) {
        if (px == 0xFF) ui_put(x, y, T_METER_E);
        else if (px >= 8 && w) { ui_put(x, y, T_METER8); px -= 8; }
        else { ui_put(x, y, (uint8_t)(T_METER0 + (px > 7 ? 7 : px))); px = 0xFF; }
        x++;
    }
}

/* Game Boy Color: each tile's palette follows from its number (gfx_cgb_attr) */
uint8_t ui_cgb;
static uint8_t attr_row[SCR_W];

void ui_cgb_init(void)
{
    uint8_t y, x;
    ui_cgb = (_cpu == CGB_TYPE);
    if (!ui_cgb) return;
    gfx_cgb_load();
    VBK_REG = 1;                                     /* attributes: palette 0 everywhere */
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) set_vram_byte((uint8_t *)(0x9800 + ((uint16_t)y << 5) + x), 0);
    VBK_REG = 0;
}

static void flush_row(uint8_t y)
{
    vr_src = (uint16_t)rowp(y);
    vr_dst = 0x9800 + ((uint16_t)y << 5);
    vr_n = SCR_W;
    vram_copy_asm();
    if (ui_cgb) {
        uint8_t x;
        const uint8_t *m = rowp(y);
        for (x = 0; x < SCR_W; x++) {
            uint8_t t = m[x];
            attr_row[x] = (t >= 96 && t < 160) ? 1 : GFX_CGB_ACCENT(t) ? 2 : 0;
        }
        vr_src = (uint16_t)attr_row;
        vr_dst = 0x9800 + ((uint16_t)y << 5);
        vr_n = SCR_W;
        VBK_REG = 1;
        vram_copy_asm();
        VBK_REG = 0;
    }
    dirty[y] = 0;
}

void ui_flush(void)
{
    uint8_t y, n = 0;
    for (y = 0; y < SCR_H; y++) {
        if (dirty[y]) {
            flush_row(y);
            if (++n >= 9) break;          /* bound per-frame work */
        }
    }
}

void ui_flush_all(void)
{
    uint8_t y;
    for (y = 0; y < SCR_H; y++) flush_row(y);
}

void ui_vram_tiles(uint8_t first, uint8_t count, const uint8_t *data)
{
    uint16_t dst = 0x8000 + ((uint16_t)first << 4);
    while (count) {
        uint8_t n = count > 8 ? 8 : count;      /* 8 tiles = 128 bytes per call */
        vr_src = (uint16_t)data;
        vr_dst = dst;
        vr_n = (uint8_t)(n << 4);
        vram_copy_asm();
        data += (uint16_t)n << 4;
        dst += (uint16_t)n << 4;
        count -= n;
    }
}

/* ---- header ------------------------------------------------------------ */
static void row1_draw(void)
{
    if (row1_on) { ui_text(0, 1, row1_buf); ui_dim(0, 1, SCR_W); }
    else ui_fill(0, 1, SCR_W, T_RULE);
}

void ui_row1(const char *s)
{
    uint8_t i = 0;
    row1_on = s != 0;
    if (s) {
        while (s[i] && i < SCR_W) { row1_buf[i] = s[i]; i++; }
        while (i < SCR_W) row1_buf[i++] = ' ';
        row1_buf[SCR_W] = 0;
        msg_timer = 0;                   /* a new title replaces a passing message */
    }
    if (!msg_timer) row1_draw();
}

/* Header (row 0):  <NAME > P01>02 F124xU>
 *   arrows: a screen exists to the left / right (SELECT+LEFT/RIGHT)
 *   P01 playing pattern, >02 queued pattern or S03 song position,
 *   fill mark, BPM, evolution on, undo available, play/stop */
void ui_header(const char *name)
{
    hdr_name = name;
    hc_cur = 0xFF;
    row1_on = 0;
    ui_fill(0, 0, SCR_W, 0);
    ui_put(0, 0, app_screen < SCR_COUNT ? T_ARROW_L : 0);
    ui_textn(1, 0, name, 5);
    ui_invert(1, 0, 5);
    ui_put(6, 0, app_screen < SCR_COUNT ? T_ARROW_R : 0);
    if (!msg_timer) row1_draw();
    ui_header_update();
}

void ui_header_update(void)
{
    uint8_t q = seq_queued;
    if (hc_cur == seq_cur && hc_q == q && hc_pos == seq_chain_pos && hc_bpm == proj.synth.bpm
        && hc_song == proj.chain.song) goto flags;
    hc_cur = seq_cur; hc_q = q; hc_pos = seq_chain_pos; hc_bpm = proj.synth.bpm; hc_song = proj.chain.song;
    ui_put(7, 0, 'P' - 32);
    ui_num0(8, 0, (uint16_t)(seq_cur + 1), 2);
    if (q != 0xFF) {
        ui_put(10, 0, '>' - 32);
        ui_num0(11, 0, (uint16_t)(q + 1), 2);
    } else if (proj.chain.song) {
        ui_put(10, 0, 'S' - 32);
        ui_num0(11, 0, (uint16_t)(seq_chain_pos + 1), 2);
    } else {
        ui_fill(10, 0, 3, 0);
    }
    ui_num(14, 0, proj.synth.bpm, 3);
flags:
    ui_put(13, 0, seq_fill_hold ? ('F' - 32 + 96) : (seq_fill ? T_FILLMARK : 0));
    ui_put(17, 0, (proj.gen.mode != EVO_STABLE) ? T_EVO : 0);
    ui_put(18, 0, (evo_undo_pat != 0xFF) ? ('U' - 32) : 0);
    ui_put(19, 0, seq_playing ? ((seq_step & 3) == 0 && (seq_step_event & 1) ? T_DOT : T_PLAY) : T_STOP);
}

void ui_msg(const char *s)
{
    uint8_t i = 0;
    while (s[i] && i < SCR_W) { msg_buf[i] = s[i]; i++; }
    while (i < SCR_W) msg_buf[i++] = ' ';
    msg_buf[SCR_W] = 0;
    msg_timer = 110;
    ui_text(0, 1, msg_buf);
}

void ui_msg_tick(void)
{
    if (msg_timer) {
        if (--msg_timer == 0) row1_draw();
    }
}

/* "<TRACK [LIVE ] GEN  >" -- where SELECT+LEFT/RIGHT lead from here (wraps) */
void ui_msg_map(uint8_t scr)
{
    uint8_t l = scr ? scr - 1 : SCR_COUNT - 1, r = scr + 1 < SCR_COUNT ? scr + 1 : 0;
    ui_fill(0, 1, SCR_W, 0);
    ui_put(0, 1, T_ARROW_L);
    ui_capn(1, 1, scr_names[l], 5);
    ui_textn(7, 1, scr_names[scr], 5);
    ui_invert(7, 1, 5);
    ui_capn(14, 1, scr_names[r], 5);
    ui_put(19, 1, T_ARROW_R);
    msg_timer = 70;
}

/* ---- input ------------------------------------------------------------- */
/* VBlank interrupt: sample the pad every frame and latch new presses, so a
 * short tap is never lost even if the main loop is busy for a few frames. */
static volatile uint8_t vbl_pad, vbl_latch;
static void vbl_isr(void)
{
    uint8_t j = joypad();
    vbl_latch |= (uint8_t)(j & ~vbl_pad);
    vbl_pad = j;
}

void ui_input_init(void)
{
    CRITICAL { add_VBL(vbl_isr); }
}

void ui_input(void)
{
    uint8_t j, latch;
    CRITICAL {
        j = vbl_pad;
        latch = vbl_latch;
        vbl_latch = 0;
    }
    key_pressed = (uint8_t)((j & ~key_prev) | latch);
    j |= latch;                      /* a tap that already ended still counts as held this frame */
    key_released = (uint8_t)(key_prev & ~j);
    key_repeat = key_pressed;
    if ((j & K_DPAD) && (j & K_DPAD) == (key_prev & K_DPAD)) {
        if (++rep_timer >= 18) {
            if (((rep_timer - 18) & 3) == 0) key_repeat |= (uint8_t)(j & K_DPAD);
            if (rep_timer > 200) rep_timer = 18;
        }
    } else {
        rep_timer = 0;
    }
    /* every press stirs the entropy pool with the exact timing of a human hand */
    if (key_pressed) ui_entropy = (uint16_t)((ui_entropy << 5) ^ (ui_entropy >> 3) ^ DIV_REG ^ ((uint16_t)TIMA_REG << 8) ^ sys_time ^ key_pressed);
    /* A pressed together with B or SELECT is a combo, never a plain tap */
    if (key_pressed & J_A) a_used = (j & (J_B | J_SELECT)) ? 1 : 0;
    if (key_pressed & J_B) b_used = (j & (J_A | J_SELECT)) ? 1 : 0;
    else if ((j & J_B) && (key_pressed & ~J_B)) b_used = 1;   /* B used as a modifier */
    if ((j & J_A) && (j & (J_B | J_SELECT))) a_used = 1;
    key_prev = j;
    key_held = j;
}

uint8_t ui_nav(void)
{
    if (key_held & (J_A | J_B | J_SELECT)) return 0;
    return key_repeat & K_DPAD;
}

int8_t ui_edit(uint8_t coarse)
{
    int8_t d = 0;
    if (!(key_held & J_A) || (key_held & (J_B | J_SELECT))) return 0;
    if (key_repeat & J_RIGHT) d = 1;
    else if (key_repeat & J_LEFT) d = -1;
    else if (key_repeat & J_UP) d = (int8_t)coarse;
    else if (key_repeat & J_DOWN) d = (int8_t)-(int8_t)coarse;
    if (d) a_used = 1;
    return d;
}

uint8_t ui_a_press(void)
{
    return (key_pressed & J_A) && !(key_held & (J_B | J_SELECT));
}

uint8_t ui_a_tap(void)
{
    return (key_released & J_A) && !a_used;
}

uint8_t ui_clear(void)
{
    if ((key_pressed & J_A) && (key_held & J_B) && !(key_held & J_SELECT)) {
        a_used = 1;
        return 1;
    }
    return 0;
}

uint8_t ui_page(void)
{
    if (!(key_held & J_B) || (key_held & (J_A | J_SELECT))) return 0;
    return key_repeat & K_DPAD;
}

uint8_t ui_b_tap(void)
{
    return (key_released & J_B) && !b_used && !(key_held & J_SELECT);
}

void ui_a_consume(void)
{
    a_used = 1;
}

uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi)
{
    int16_t r = (int16_t)v + d;
    if (r < lo) r = lo;
    if (r > hi) r = hi;
    return (uint8_t)r;
}

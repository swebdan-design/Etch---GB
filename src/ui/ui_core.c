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

uint8_t key_held, key_pressed, key_repeat;
static uint8_t key_prev, rep_timer;

static char msg_buf[SCR_W + 1];
static uint8_t msg_timer;
static const char *hdr_name;
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

void ui_text(uint8_t x, uint8_t y, const char *s)
{
    while (*s && x < SCR_W) {
        uint8_t c = (uint8_t)*s++;
        ui_put(x++, y, c >= 32 && c < 128 ? (uint8_t)(c - 32) : 0);
    }
}

void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n)
{
    while (n--) {
        uint8_t c = (uint8_t)*s;
        if (c) s++;
        ui_put(x++, y, c >= 32 && c < 128 ? (uint8_t)(c - 32) : 0);
    }
}

void ui_invert(uint8_t x, uint8_t y, uint8_t w)
{
    while (w--) {
        uint8_t t = map[y][x];
        if (t < 64) ui_put(x, y, (uint8_t)(t + 96));
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
    while (w--) {
        if (px >= 8) { ui_put(x, y, T_METER8); px -= 8; }
        else { ui_put(x, y, (uint8_t)(T_METER0 + px)); px = 0; }
        x++;
    }
}

static void flush_row(uint8_t y)
{
    vr_src = (uint16_t)rowp(y);
    vr_dst = 0x9800 + ((uint16_t)y << 5);
    vr_n = SCR_W;
    vram_copy_asm();
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
void ui_header(const char *name)
{
    hdr_name = name;
    hc_cur = 0xFF;
    ui_fill(0, 0, SCR_W, 0);
    ui_textn(0, 0, name, 5);
    ui_invert(0, 0, 5);
    ui_fill(0, 1, SCR_W, T_RULE_HEAVY);
    ui_header_update();
}

void ui_header_update(void)
{
    uint8_t q = seq_queued;
    if (hc_cur == seq_cur && hc_q == q && hc_pos == seq_chain_pos && hc_bpm == proj.synth.bpm
        && hc_song == proj.chain.song) goto flags;
    hc_cur = seq_cur; hc_q = q; hc_pos = seq_chain_pos; hc_bpm = proj.synth.bpm; hc_song = proj.chain.song;
    ui_put(6, 0, 'P' - 32);
    ui_num0(7, 0, (uint16_t)(seq_cur + 1), 2);
    if (q != 0xFF) {
        ui_put(9, 0, T_ARROW_R);
        ui_num0(10, 0, (uint16_t)(q + 1), 2);
    } else if (proj.chain.song) {
        ui_put(9, 0, 'S' - 32);
        ui_num0(10, 0, (uint16_t)(seq_chain_pos + 1), 2);
    } else {
        ui_fill(9, 0, 3, 0);
    }
    ui_num(13, 0, proj.synth.bpm, 3);
flags:
    ui_put(12, 0, seq_fill ? T_FILLMARK : 0);
    ui_put(16, 0, (proj.gen.mode != EVO_STABLE) ? T_EVO : 0);
    ui_put(17, 0, (evo_undo_pat != 0xFF) ? ('U' - 32) : 0);
    ui_put(19, 0, seq_playing ? T_PLAY : T_STOP);
    /* beat blink on the play icon column 18 */
    ui_put(18, 0, (seq_playing && (seq_step & 3) == 0) ? T_DOT : 0);
}

void ui_msg(const char *s)
{
    uint8_t i = 0;
    while (s[i] && i < SCR_W) { msg_buf[i] = s[i]; i++; }
    while (i < SCR_W) msg_buf[i++] = ' ';
    msg_buf[SCR_W] = 0;
    msg_timer = 110;
    ui_text(0, 1, msg_buf);
    ui_invert(0, 1, SCR_W);
}

void ui_msg_tick(void)
{
    if (msg_timer) {
        if (--msg_timer == 0) ui_fill(0, 1, SCR_W, T_RULE_HEAVY);
    }
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
    key_repeat = key_pressed;
    if ((j & K_DPAD) && (j & K_DPAD) == (key_prev & K_DPAD)) {
        if (++rep_timer >= 18) {
            if (((rep_timer - 18) & 3) == 0) key_repeat |= (uint8_t)(j & K_DPAD);
            if (rep_timer > 200) rep_timer = 18;
        }
    } else {
        rep_timer = 0;
    }
    key_prev = j;
    key_held = j;
}

int8_t ui_adjust(uint8_t coarse)
{
    if (key_repeat & J_UP) return 1;
    if (key_repeat & J_DOWN) return -1;
    if (key_repeat & J_RIGHT) return (int8_t)coarse;
    if (key_repeat & J_LEFT) return (int8_t)-(int8_t)coarse;
    return 0;
}

uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi)
{
    int16_t r = (int16_t)v + d;
    if (r < lo) r = lo;
    if (r > hi) r = hi;
    return (uint8_t)r;
}

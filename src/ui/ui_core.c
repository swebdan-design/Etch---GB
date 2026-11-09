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

extern uint16_t vr_src, vr_dst;
extern uint8_t vr_n;
void vram_copy_asm(void);

uint8_t key_held, key_pressed, key_repeat;
static uint8_t key_prev, rep_timer;

static char msg_buf[SCR_W + 1];
static uint8_t msg_timer;
static const char *hdr_name;

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
    if (x >= SCR_W || y >= SCR_H) return;
    if (map[y][x] != t) {
        map[y][x] = t;
        dirty[y] = 1;
    }
}

uint8_t ui_get(uint8_t x, uint8_t y)
{
    return map[y][x];
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
    uint8_t i = digits;
    uint8_t lead = 1;
    while (i--) {
        uint8_t d = (uint8_t)(v % 10);
        ui_put((uint8_t)(x + i), y, (v || lead) ? (uint8_t)('0' - 32 + d) : 0);
        v /= 10;
        lead = 0;
    }
}

void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits)
{
    uint8_t i = digits;
    while (i--) {
        ui_put((uint8_t)(x + i), y, (uint8_t)('0' - 32 + (uint8_t)(v % 10)));
        v /= 10;
    }
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
    uint8_t pc = note % 12;
    ui_put(x, y, (uint8_t)(note_names[pc * 2] - 32));
    ui_put((uint8_t)(x + 1), y, (uint8_t)(note_names[pc * 2 + 1] - 32));
    ui_put((uint8_t)(x + 2), y, (uint8_t)('1' - 32 + note / 12));
}

void ui_meter(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max)
{
    /* w tiles, 8 px each */
    uint16_t px = max ? (uint16_t)(((uint16_t)v * (uint16_t)(w * 8) + max / 2) / max) : 0;
    while (w--) {
        if (px >= 8) { ui_put(x, y, T_METER8); px -= 8; }
        else { ui_put(x, y, (uint8_t)(T_METER0 + px)); px = 0; }
        x++;
    }
}

static void flush_row(uint8_t y)
{
    vr_src = (uint16_t)&map[y][0];
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
    ui_fill(0, 0, SCR_W, 0);
    ui_textn(0, 0, name, 5);
    ui_invert(0, 0, 5);
    ui_fill(0, 1, SCR_W, T_RULE_HEAVY);
    ui_header_update();
}

void ui_header_update(void)
{
    uint8_t q = seq_queued;
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
    ui_put(12, 0, seq_fill ? T_FILLMARK : 0);
    ui_num(13, 0, proj.synth.bpm, 3);
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
void ui_input(void)
{
    uint8_t j = joypad();
    key_pressed = (uint8_t)(j & ~key_prev);
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

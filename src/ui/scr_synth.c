/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * SYNTH: acid voice parameters + live scope of the wavetable being played.
 *
 *   UP/DOWN          select parameter
 *   LEFT/RIGHT       adjust by 1      B+UP/DOWN adjust by 1
 *   B+LEFT/RIGHT     coarse adjust    A  audition (when stopped)
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

#define N_PARAMS 16
static const char * const lbl[N_PARAMS] = {
    "WAVE  ", "CUTOFF", "RESO  ", "ENVMOD", "DECAY ", "ACCENT", "DRIVE ", "SLIDE ",
    "GATE  ", "LFO RT", "LFO DP", "LFO TO", "LFO SH", "BPM   ", "SWING ", "TUNE  "
};
static const uint8_t pmax[N_PARAMS] = { 3, 31, 7, 15, 15, 15, 2, 15, 3, 15, 15, 3, 3, 220, 8, 24 };
static const char * const wave_n[4]  = { "SAW ", "SQR ", "HOLW", "BITE" };
static const char * const drive_n[3] = { "OFF ", "SOFT", "HARD" };
static const char * const gate_n[4]  = { "SHRT", "MED ", "LONG", "FULL" };
static const char * const dest_n[4]  = { "OFF ", "PTCH", "CUT ", "RES " };
static const char * const shape_n[4] = { "TRI ", "SQR ", "SAW ", "S&H " };

static uint8_t cur, scope_div, sc_w = 0xFF, sc_c, sc_r;
static uint8_t scope_buf[128];

/* screen order -> byte offset inside synth_t */
static const uint8_t poff[N_PARAMS] = { 0, 1, 2, 3, 4, 5, 6, 7, 15, 8, 9, 10, 11, 12, 13, 14 };

static uint8_t *pptr(uint8_t i)
{
    return (uint8_t *)&proj.synth + poff[i];
}

static uint8_t pget(uint8_t i)
{
    return *pptr(i);
}

static void draw_param(uint8_t i)
{
    uint8_t y = (uint8_t)(2 + i), v = pget(i);
    const char *name = 0;
    ui_text(0, y, lbl[i]);
    if (cur == i) ui_invert(0, y, 6);
    switch (i) {
    case 0: name = wave_n[v & 3]; break;
    case 6: name = drive_n[v > 2 ? 2 : v]; break;
    case 8: name = gate_n[v & 3]; break;
    case 11: name = dest_n[v & 3]; break;
    case 12: name = shape_n[v & 3]; break;
    }
    if (name) ui_textn(6, y, name, 4);
    else if (i == 15) {
        ui_put(6, y, v >= 12 ? ('+' - 32) : ('-' - 32));
        ui_num(7, y, v >= 12 ? (uint16_t)(v - 12) : (uint16_t)(12 - v), 2);
        ui_put(9, y, 0);
    } else {
        ui_num(6, y, v, 3);
        ui_put(9, y, 0);
    }
    if (i == 13) ui_meter(10, y, 4, (uint8_t)(v - 60), 160);
    else ui_meter(10, y, 4, v, pmax[i]);
}

static void scope_render(uint8_t wave, uint8_t res, uint8_t cut)
{
    uint8_t raw[16], smp[32], i, x, y, y0, y1, prev;
    synth_wave_peek(wave, res, cut, raw);
    for (i = 0; i < 16; i++) {
        smp[i * 2] = raw[i] >> 4;
        smp[i * 2 + 1] = raw[i] & 15;
    }
    memset(scope_buf, 0, sizeof(scope_buf));
    /* faint centre line */
    for (x = 0; x < 32; x++) {
        if (x & 1) {
            uint8_t *t = &scope_buf[((8 >> 3) * 4 + (x >> 3)) * 16 + ((8 & 7) << 1)];
            t[0] |= (uint8_t)(0x80 >> (x & 7));   /* colour 1 */
        }
    }
    prev = (uint8_t)(15 - smp[0]);
    for (x = 0; x < 32; x++) {
        y = (uint8_t)(15 - smp[x]);
        y0 = y < prev ? y : prev;
        y1 = y < prev ? prev : y;
        if (x == 0) { y0 = y1 = y; }
        for (; y0 <= y1; y0++) {
            uint8_t *t = &scope_buf[((y0 >> 3) * 4 + (x >> 3)) * 16 + ((y0 & 7) << 1)];
            uint8_t bit = (uint8_t)(0x80 >> (x & 7));
            t[0] |= bit;
            t[1] |= bit;
        }
        prev = y;
    }
    ui_vram_tiles(T_SCOPE_BASE, 8, scope_buf);
}

static void draw_scope_frame(void)
{
    uint8_t i;
    ui_text(15, 2, "SCOPE");
    for (i = 0; i < 4; i++) {
        ui_put((uint8_t)(15 + i), 3, (uint8_t)(T_SCOPE_BASE + i));
        ui_put((uint8_t)(15 + i), 4, (uint8_t)(T_SCOPE_BASE + 4 + i));
    }
    ui_put(14, 3, T_VBAR);
    ui_put(14, 4, T_VBAR);
    ui_put(19, 3, T_VBAR);
    ui_put(19, 4, T_VBAR);
}

static void draw_all(void)
{
    uint8_t i;
    ui_cls();
    ui_header("SYNTH");
    for (i = 0; i < N_PARAMS; i++) draw_param(i);
    draw_scope_frame();
    sc_w = 0xFF;
}

void scr_synth_enter(void) BANKED
{
    draw_all();
}

static void adjust(uint8_t i, int8_t d)
{
    switch (i) {
    case 13:
        proj.synth.bpm = clampadd(proj.synth.bpm, (int8_t)(d > 1 ? 10 : (d < -1 ? -10 : d)), 60, 220);
        seq_set_tempo();
        break;
    case 14:
        proj.synth.swing = clampadd(proj.synth.swing, d > 0 ? 1 : -1, 0, 8);
        seq_set_tempo();
        break;
    case 15:
        proj.synth.tune = clampadd(proj.synth.tune, (int8_t)(d > 1 ? 12 : (d < -1 ? -12 : d)), 0, 24);
        break;
    default: {
        uint8_t *p = pptr(i);
        int8_t dd = d;
        if (pmax[i] <= 7) dd = d > 0 ? 1 : -1;
        *p = clampadd(*p, dd, 0, pmax[i]);
        break;
    }
    }
    draw_param(i);
}

void scr_synth_frame(void) BANKED
{
    uint8_t old = cur, cut, res;
    int8_t d = 0;
    if (!(key_held & J_SELECT)) {
        if (key_held & J_B) {
            d = ui_adjust(4);
        } else {
            if (key_repeat & J_UP) cur = cur ? (uint8_t)(cur - 1) : (uint8_t)(N_PARAMS - 1);
            if (key_repeat & J_DOWN) cur = (uint8_t)((cur + 1) % N_PARAMS);
            if (key_repeat & J_RIGHT) d = 1;
            if (key_repeat & J_LEFT) d = -1;
            if (key_pressed & J_A) app_audition(proj.pat[seq_cur].note[0], 1);
        }
    }
    if (old != cur) { draw_param(old); draw_param(cur); }
    if (d) adjust(cur, d);

    /* live scope: the table the engine is actually playing (or the base
     * setting while the voice is silent) */
    if (++scope_div >= 3) {
        scope_div = 0;
        if (synth_gate) { cut = synth_dbg_cut; res = synth_dbg_res; }
        else { cut = proj.synth.cutoff; res = proj.synth.res; }
        if (sc_w != (proj.synth.wave & 3) || sc_c != cut || sc_r != res) {
            sc_w = proj.synth.wave & 3; sc_c = cut; sc_r = res;
            scope_render(proj.synth.wave & 3, res, cut);
            ui_put(15, 5, 'C' - 32);
            ui_num(16, 5, cut, 2);
            ui_put(18, 5, 'R' - 32);
            ui_put(19, 5, (uint8_t)('0' - 32 + res));
        }
    }
}

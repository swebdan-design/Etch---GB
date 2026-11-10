/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * LIVE: the performance screen.
 *
 *   rows 3-7   lane overview (bass + 4 drum lanes) with playhead and activity
 *   rows 8-16  performance macros: CUT RES ENV DEC ACC DRV BPM EVO MUT
 *
 *   UP/DOWN      select a lane or macro
 *   LEFT/RIGHT   lane: mute/unmute   macro: adjust (hold to repeat)
 *   B+LEFT/RIGHT coarse adjust
 *   A            FILL (drum fill until the next bar line)
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"
#include "corrosion.h"

#define ROW_LANE0  3
#define ROW_MACRO0 8
#define N_LANES    5
#define N_MACROS   9
#define N_ROWS     (N_LANES + N_MACROS)

static uint8_t cur;
static uint8_t last_event;
static uint8_t act[N_LANES];

static const char * const lane_lbl[N_LANES] = { "BS", "KK", "SN", "CH", "OH" };
static const char * const macro_lbl[N_MACROS] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "BPM", "EVO", "MUT" };
static const char * const drive_names[3] = { "OFF ", "SOFT", "HARD" };
static const char * const wave_names[4] = { "SAW ", "SQR ", "HOLW", "BITE" };

static void draw_lanes(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    const uint16_t *mk = seq_fill ? seq_fill_mask : p->drum;
    uint8_t s, l, x, f, t, tile;
    uint8_t lm[NUM_LANES * 2];
    for (l = 0; l < NUM_LANES; l++) {
        lm[l * 2] = (uint8_t)mk[l];
        lm[l * 2 + 1] = (uint8_t)(mk[l] >> 8);
    }
    for (s = 0; s < 16; s++) {
        x = (uint8_t)(3 + s);
        f = p->flags[s];
        t = f & STEP_TYPE_MASK;
        if (t == STEP_NOTE) {
            tile = (f & STEP_ACCENT) ? T_CELL_ACC : (f & STEP_SLIDE) ? T_CELL_SLIDE : T_CELL_ON;
        } else if (t == STEP_TIE) {
            tile = T_CELL_TIE;
        } else {
            tile = (s & 3) ? T_CELL_OFF : T_CELL_BEAT;
        }
        ui_put(x, ROW_LANE0, tile);
        {
            uint8_t off = (s & 3) ? T_CELL_OFF : T_CELL_BEAT;
            uint8_t byte = s >> 3, bit = (uint8_t)(1 << (s & 7));
            for (l = 0; l < NUM_LANES; l++)
                ui_put(x, (uint8_t)(ROW_LANE0 + 1 + l), (lm[l * 2 + byte] & bit) ? T_CELL_ON : off);
        }
    }
}

static void draw_playhead(void)
{
    uint8_t s;
    for (s = 0; s < 16; s++)
        ui_put((uint8_t)(3 + s), 2, (seq_playing && s == seq_step) ? T_PLAYHEAD : ((s & 3) ? 0 : T_DOT));
}

static uint8_t macro_value(uint8_t m, uint8_t *max)
{
    switch (m) {
    case 0: *max = 31; return proj.synth.cutoff;
    case 1: *max = 7;  return proj.synth.res;
    case 2: *max = 15; return proj.synth.envmod;
    case 3: *max = 15; return proj.synth.decay;
    case 4: *max = 15; return proj.synth.accent;
    case 5: *max = 2;  return proj.synth.drive;
    case 6: *max = 160; return (uint8_t)(proj.synth.bpm - 60);
    case 7: *max = 2;  return proj.gen.mode;
    default: *max = 15; return proj.gen.mutation;
    }
}

static void draw_macro(uint8_t m)
{
    uint8_t y = (uint8_t)(ROW_MACRO0 + m), max, v;
    v = macro_value(m, &max);
    ui_text(0, y, macro_lbl[m]);
    ui_put(3, y, 0);
    ui_meter(4, y, 8, v, max);
    ui_put(12, y, 0);
    if (m == 5) ui_textn(13, y, drive_names[v], 7);
    else if (m == 6) { ui_num(13, y, proj.synth.bpm, 3); ui_textn(16, y, " BPM", 4); }
    else if (m == 7) ui_textn(13, y, corr_mode_names[v], 7);
    else { ui_num(13, y, v, 3); ui_fill(16, y, 4, 0); }
    if (cur == N_LANES + m) ui_invert(0, y, 3);
}

static void draw_lane_labels(void)
{
    uint8_t l, y;
    for (l = 0; l < N_LANES; l++) {
        y = (uint8_t)(ROW_LANE0 + l);
        ui_text(0, y, lane_lbl[l]);
        if (cur == l) ui_invert(0, y, 2);
        if (proj.mutes & (1 << l)) ui_put(2, y, T_MUTE);
        else ui_put(2, y, act[l] ? (uint8_t)(T_ACT0 + (act[l] >> 1)) : 0);
        ui_put(19, y, 0);
    }
}

static void draw_footer(void)
{
    ui_text(0, 17, "WAVE");
    ui_textn(5, 17, wave_names[proj.synth.wave & 3], 4);
    ui_text(10, 17, "GEN");
    ui_num0(14, 17, proj.pat[seq_cur].gen, 3);
    ui_put(17, 17, 0);
    ui_put(18, 17, proj.pat[seq_cur].lock ? T_LOCK : 0);
    ui_put(19, 17, 0);
}

static void draw_all(void)
{
    uint8_t m;
    ui_cls();
    ui_header("LIVE");
    draw_playhead();
    draw_lane_labels();
    draw_lanes();
    for (m = 0; m < N_MACROS; m++) draw_macro(m);
    draw_footer();
}

void scr_live_enter(void) BANKED
{
    last_event = seq_step_event;
    draw_all();
}

static void adjust(uint8_t m, int8_t d)
{
    switch (m) {
    case 0: proj.synth.cutoff = clampadd(proj.synth.cutoff, d, 0, 31); break;
    case 1: proj.synth.res = clampadd(proj.synth.res, d > 1 ? 1 : (d < -1 ? -1 : d), 0, 7); break;
    case 2: proj.synth.envmod = clampadd(proj.synth.envmod, d, 0, 15); break;
    case 3: proj.synth.decay = clampadd(proj.synth.decay, d, 0, 15); break;
    case 4: proj.synth.accent = clampadd(proj.synth.accent, d, 0, 15); break;
    case 5: proj.synth.drive = clampadd(proj.synth.drive, d > 0 ? 1 : -1, 0, 2); break;
    case 6:
        proj.synth.bpm = clampadd(proj.synth.bpm, (int8_t)(d > 1 ? 10 : (d < -1 ? -10 : d)), 60, 220);
        seq_set_tempo();
        break;
    case 7: proj.gen.mode = clampadd(proj.gen.mode, d > 0 ? 1 : -1, 0, 2); evo_state = EVO_IDLE; break;
    default: proj.gen.mutation = clampadd(proj.gen.mutation, d, 0, 15); evo_state = EVO_IDLE; break;
    }
    draw_macro(m);
}

void scr_live_frame(void) BANKED
{
    uint8_t l, old = cur;
    int8_t d = 0;

    /* ---- input --------------------------------------------------------- */
    if (!(key_held & J_SELECT)) {
        if (key_repeat & J_UP) cur = cur ? (uint8_t)(cur - 1) : (uint8_t)(N_ROWS - 1);
        if (key_repeat & J_DOWN) cur = (uint8_t)((cur + 1) % N_ROWS);
        if (key_repeat & J_RIGHT) d = (key_held & J_B) ? 4 : 1;
        if (key_repeat & J_LEFT) d = (key_held & J_B) ? -4 : -1;
        if (key_pressed & J_A) app_fill();
    }
    if (cur != old) {
        draw_lane_labels();
        if (old >= N_LANES) draw_macro((uint8_t)(old - N_LANES));
        if (cur >= N_LANES) draw_macro((uint8_t)(cur - N_LANES));
    }
    if (d) {
        if (cur < N_LANES) {
            proj.mutes ^= (uint8_t)(1 << cur);
            if (cur == 0 && (proj.mutes & 1)) synth_gate_off();
        } else {
            adjust((uint8_t)(cur - N_LANES), d);
        }
    }

    /* ---- dynamic display ------------------------------------------------ */
    if (seq_step_event != last_event) {
        uint8_t a = seq_activity;
        last_event = seq_step_event;
        for (l = 0; l < N_LANES; l++) if (a & (1 << l)) act[l] = 9;
        draw_playhead();
        draw_lanes();
        draw_footer();
    }
    {
        uint8_t ch = 0;
        for (l = 0; l < N_LANES; l++) if (act[l]) { act[l]--; ch = 1; }
        if (ch) draw_lane_labels();
    }
    if (!seq_playing) draw_playhead();
}

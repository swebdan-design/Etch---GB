/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * LIVE: the home screen.  Overview, basic step sequencing and performance.
 *
 *   rows 3-7   BS KK SN CH OH lanes, 16 steps each (editable)
 *   rows 8-16  performance macros: CUT RES ENV DEC ACC DRV RGN EVO MUT
 *              (RGN = REGEN: A replaces exactly half the bar with new material;
 *               BPM lives on the SONG screen)
 *   row 17     context help for whatever the cursor is on
 *
 *   D-pad            move (left of step 1 = the lane name)
 *   lane name  A     mute / unmute the lane
 *   bass step  A     add a note (the last note used) / tap again to remove
 *              A+</> semitone      A+UP/DOWN octave      B+A remove
 *   drum step  A     hit on / off                         B+A remove
 *   macro      </>   adjust (hold to sweep), A+UP/DOWN coarse,  A tap = FILL
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

static uint8_t cy, cx = 1;           /* cx 0 = lane name, 1..16 = steps */
static uint8_t last_event;
static uint8_t act[N_LANES];
static uint8_t edited_here;          /* A press already acted on this step */

static const char * const lane_lbl[N_LANES] = { "BS", "KK", "SN", "CH", "OH" };
static const char * const lane_long[N_LANES] = { "BASS ", "KICK ", "SNARE", "HAT  ", "OPEN " };
static const char * const macro_lbl[N_MACROS] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "RGN", "EVO", "MUT" };
#define M_RGN 6
static uint8_t regen_what;            /* 0 ALL, 1 BASS, 2 DRUMS */
static const char * const regen_n[3] = { "ALL  ", "BASS ", "DRUMS" };
static const uint8_t regen_gen[3] = { GEN_ALL, GEN_BASS, GEN_DRUMS };
static const char * const drive_names[3] = { "OFF ", "SOFT", "HARD" };

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
        uint8_t off = (s & 3) ? T_CELL_OFF : T_CELL_BEAT;
        uint8_t byte = s >> 3, bit = (uint8_t)(1 << (s & 7));
        uint8_t cur = (cy < N_LANES && cx == s + 1);
        x = (uint8_t)(3 + s);
        f = p->flags[s];
        t = f & STEP_TYPE_MASK;
        if (t == STEP_NOTE) tile = (f & STEP_ACCENT) ? T_CELL_ACC : (f & STEP_SLIDE) ? T_CELL_SLIDE : T_CELL_ON;
        else if (t == STEP_TIE) tile = T_CELL_TIE;
        else tile = off;
        if (cur && cy == 0) tile = (t == STEP_REST) ? T_CELL_CUR_OFF : T_CELL_CUR_ON;
        ui_put(x, ROW_LANE0, tile);
        for (l = 0; l < NUM_LANES; l++) {
            uint8_t on = (lm[l * 2 + byte] & bit) != 0;
            tile = on ? T_CELL_ON : off;
            if (cur && cy == l + 1) tile = on ? T_CELL_CUR_ON : T_CELL_CUR_OFF;
            ui_put(x, (uint8_t)(ROW_LANE0 + 1 + l), tile);
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
    case 6: *max = 2; return regen_what;
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
    else if (m == M_RGN) {
        ui_textn(4, y, "50% NEW ", 8);         /* no meter: it is an action */
        ui_textn(13, y, regen_n[regen_what], 7);
    }
    else if (m == 7) ui_textn(13, y, corr_mode_names[v], 7);
    else { ui_num(13, y, v, 3); ui_fill(16, y, 4, 0); }
    if (cy == N_LANES + m) ui_invert(0, y, 3);
}

static void draw_lane_labels(void)
{
    uint8_t l, y;
    for (l = 0; l < N_LANES; l++) {
        y = (uint8_t)(ROW_LANE0 + l);
        ui_text(0, y, lane_lbl[l]);
        if (cy == l && cx == 0) ui_invert(0, y, 2);
        if (proj.mutes & (1 << l)) ui_put(2, y, T_MUTE);
        else ui_put(2, y, act[l] ? (uint8_t)(T_ACT0 + (act[l] >> 1)) : 0);
        ui_put(19, y, cy == l ? T_ARROW_L : 0);
    }
}

/* row 17: what A does here */
static void draw_help(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    ui_fill(0, 17, SCR_W, 0);
    if (cy == N_LANES + 7) {             /* EVO: say what it does */
        static const char * const evo_help[3] = {
            "STABLE: NO CHANGES  ", "EVOLVES EVERY 4 BARS", "MUTATES EVERY 2 BARS" };
        ui_text(0, 17, evo_help[proj.gen.mode % 3]);
        return;
    }
    if (cy == N_LANES + M_RGN) {
        ui_text(0, 17, "A:REGEN  </>:WHAT");
        return;
    }
    if (cy >= N_LANES) {
        /* one line per macro: what it does, and A plays a fill */
        static const char * const mhelp[N_MACROS] = {
            "DARK<>BRIGHT A:FILL", "SQUELCH      A:FILL", "FILTER WOW   A:FILL",
            "SWEEP LENGTH A:FILL", "ACCENT PUNCH A:FILL", "DISTORTION   A:FILL",
            "", "", "HOW FAR IT WANDERS " };
        ui_text(0, 17, mhelp[cy - N_LANES]);
        return;
    }
    if (cx == 0) {
        ui_textn(0, 17, lane_long[cy], 5);
        ui_text(6, 17, (proj.mutes & (1 << cy)) ? "MUTED  A:UNMUTE" : "ON     A:MUTE");
        return;
    }
    ui_put(0, 17, 'S' - 32);
    ui_num0(1, 17, cx, 2);
    if (cy == 0) {
        uint8_t s = (uint8_t)(cx - 1), t = p->flags[s] & STEP_TYPE_MASK;
        if (t == STEP_NOTE) {
            ui_note(4, 17, p->note[s]);
            ui_text(8, 17, "A+DPAD:PITCH");
        } else if (t == STEP_TIE) {
            ui_text(4, 17, "TIE A:REMOVE");
        } else {
            ui_text(4, 17, "--- A:NOTE ");
            ui_note(15, 17, app_last_note);
        }
    } else {
        ui_textn(4, 17, lane_long[cy], 5);
        ui_text(10, 17, "A:HIT");
    }
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
    draw_help();
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
    case 1: proj.synth.res = clampadd(proj.synth.res, d > 0 ? 1 : -1, 0, 7); break;
    case 2: proj.synth.envmod = clampadd(proj.synth.envmod, d, 0, 15); break;
    case 3: proj.synth.decay = clampadd(proj.synth.decay, d, 0, 15); break;
    case 4: proj.synth.accent = clampadd(proj.synth.accent, d, 0, 15); break;
    case 5: proj.synth.drive = clampadd(proj.synth.drive, d > 0 ? 1 : -1, 0, 2); break;
    case M_RGN:
        regen_what = (uint8_t)((regen_what + (d > 0 ? 1 : 2)) % 3);
        break;
    case 7: proj.gen.mode = clampadd(proj.gen.mode, d > 0 ? 1 : -1, 0, 2); evo_state = EVO_IDLE; draw_help(); break;
    default: proj.gen.mutation = clampadd(proj.gen.mutation, d, 0, 15); evo_state = EVO_IDLE; break;
    }
    draw_macro(m);
}

/* REGEN: replace exactly half of the bar (8 random steps) with new
 * material in the GEN key / scale / style.  Instant, permanent, undoable. */
static void regen(void)
{
    if (proj.pat[seq_cur].lock & PLOCK_ALL) { ui_msg("PATTERN LOCKED"); return; }
    if (app_regen(regen_gen[regen_what])) ui_msg("REGEN: 8 STEPS NEW");
    else ui_msg("ALL LOCKED");
    draw_lanes();
}

/* bass step edit: d = semitones (0 = just make it a note) */
static void bass_set(pattern_t *p, uint8_t s, int8_t d)
{
    uint8_t f = p->flags[s];
    if ((f & STEP_TYPE_MASK) != STEP_NOTE) {
        p->note[s] = app_last_note;
        p->flags[s] = (uint8_t)((f & ~STEP_TYPE_MASK) | STEP_NOTE);
    }
    p->note[s] = clampadd(p->note[s], d, NOTE_MIN, NOTE_MAX);
    app_last_note = p->note[s];
    app_audition(p->note[s], p->flags[s] & STEP_ACCENT);
}

void scr_live_frame(void) BANKED
{
    pattern_t *p = &proj.pat[seq_cur];
    uint8_t l, ocy = cy, ocx = cx, nav, changed = 0;
    int8_t d;

    /* ---- navigation ----------------------------------------------------- */
    nav = ui_nav();
    if (nav & J_UP) cy = cy ? (uint8_t)(cy - 1) : (uint8_t)(N_ROWS - 1);
    if (nav & J_DOWN) cy = (uint8_t)((cy + 1) % N_ROWS);
    if (cy < N_LANES) {
        if (nav & J_LEFT) cx = cx ? (uint8_t)(cx - 1) : 16;
        if (nav & J_RIGHT) cx = cx < 16 ? (uint8_t)(cx + 1) : 0;
    } else if (nav & (J_LEFT | J_RIGHT)) {
        adjust((uint8_t)(cy - N_LANES), (nav & J_RIGHT) ? 1 : -1);   /* plain </> sweeps macros */
    }

    /* ---- editing -------------------------------------------------------- */
    if (cy >= N_LANES) {
        d = ui_edit(4);
        if (d) adjust((uint8_t)(cy - N_LANES), d);
        if (ui_a_tap()) {
            if (cy == N_LANES + M_RGN) regen();
            else app_fill();
        }
    } else if (cx == 0) {
        if (ui_a_press()) {
            ui_a_consume();
            proj.mutes ^= (uint8_t)(1 << cy);
            if (cy == 0 && (proj.mutes & 1)) synth_gate_off();
            changed = 1;
        }
    } else {
        uint8_t s = (uint8_t)(cx - 1);
        if (cy == 0) {
            if (ui_a_press()) {
                edited_here = 0;
                if ((p->flags[s] & STEP_TYPE_MASK) == STEP_REST) {
                    bass_set(p, s, 0);
                    edited_here = 1;
                    changed = 1;
                }
            }
            d = ui_edit(12);
            if (d) { bass_set(p, s, d); changed = 1; }
            if (ui_a_tap() && !edited_here) {
                p->flags[s] &= (uint8_t)~(STEP_TYPE_MASK | STEP_SLIDE | STEP_ACCENT);
                changed = 1;
            }
            if (ui_clear()) {
                p->flags[s] &= (uint8_t)~(STEP_TYPE_MASK | STEP_SLIDE | STEP_ACCENT);
                changed = 1;
            }
        } else {
            l = (uint8_t)(cy - 1);
            if (ui_a_press()) {
                ui_a_consume();
                p->drum[l] ^= (uint16_t)1 << s;
                if (!seq_playing && (p->drum[l] & ((uint16_t)1 << s))) drums_play((uint8_t)(1 << l), p->kit);
                changed = 1;
            }
            if (ui_clear()) { p->drum[l] &= (uint16_t)~((uint16_t)1 << s); changed = 1; }
        }
    }
    if (changed) app_pattern_edited();

    if (cy != ocy || cx != ocx || changed) {
        draw_lane_labels();
        draw_lanes();
        if (ocy >= N_LANES) draw_macro((uint8_t)(ocy - N_LANES));
        if (cy >= N_LANES) draw_macro((uint8_t)(cy - N_LANES));
        draw_help();
    }

    /* ---- dynamic display ------------------------------------------------ */
    if (seq_step_event != last_event) {
        uint8_t a = seq_activity;
        last_event = seq_step_event;
        for (l = 0; l < N_LANES; l++) if (a & (1 << l)) act[l] = 9;
        draw_playhead();
        draw_lanes();
    }
    {
        uint8_t ch = 0;
        for (l = 0; l < N_LANES; l++) if (act[l]) { act[l]--; ch = 1; }
        if (ch) draw_lane_labels();
    }
    if (!seq_playing) draw_playhead();
}

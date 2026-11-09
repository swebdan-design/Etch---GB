/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * GRID: 16-step bass editor (one row per step, tracker style).
 *
 *   col: >  ST  NOTE  A  S  P  CND  L
 *   UP/DOWN        select step          LEFT/RIGHT   select field
 *   NOTE  A        cycle REST -> NOTE -> TIE
 *         B+UP/DN  semitone             B+LEFT/RIGHT octave
 *   A/S/L A        toggle accent / slide / lock
 *   P     B+UP/DN  probability 1..8 (8 = always), A = always
 *   CND   B+UP/DN  condition, A = ALL
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

enum { F_NOTE, F_ACC, F_SLIDE, F_PROB, F_COND, F_LOCK, F_COUNT };
static const uint8_t field_x[F_COUNT] = { 4, 8, 10, 12, 14, 18 };
static const uint8_t field_w[F_COUNT] = { 3, 1, 1, 1, 3, 1 };
static const char * const cond_names[COND_COUNT] = {
    "ALL", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4", "FIL", "!FL"
};

static uint8_t cs, cf;          /* cursor step, field */
static uint8_t last_step = 0xFF;
static uint8_t last_pat = 0xFF, last_commits;

static void draw_step(uint8_t s)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t y = (uint8_t)(2 + s), f = p->flags[s], t = f & STEP_TYPE_MASK;
    uint8_t pr = p->prob[s];
    ui_put(0, y, (seq_playing && seq_step == s) ? T_PLAY : 0);
    ui_num0(1, y, (uint16_t)(s + 1), 2);
    ui_put(3, y, 0);
    if (t == STEP_NOTE) ui_note(4, y, p->note[s]);
    else if (t == STEP_TIE) ui_fill(4, y, 3, T_TIE_DASH);
    else ui_text(4, y, "---");
    ui_put(7, y, 0);
    ui_put(8, y, (f & STEP_ACCENT) ? T_ACCENT : T_DOT);
    ui_put(9, y, 0);
    ui_put(10, y, (f & STEP_SLIDE) ? T_SLIDE : T_DOT);
    ui_put(11, y, 0);
    ui_put(12, y, (uint8_t)('1' - 32 + (pr & 7)));
    ui_put(13, y, 0);
    ui_text(14, y, cond_names[(pr >> 4) < COND_COUNT ? (pr >> 4) : 0]);
    ui_fill(17, y, 1, 0);
    ui_put(18, y, (f & STEP_LOCK) ? T_LOCK : T_DOT);
    ui_put(19, y, 0);
    if (s == cs) {
        uint8_t x = field_x[cf], w = field_w[cf];
        uint8_t tile0 = ui_get(x, y);
        if (w == 1 && tile0 >= T_SYM_BASE) {
            /* symbol fields: show the cursor as an inverted letter */
            static const char sym_letter[F_COUNT] = { 0, 'A', 'S', 0, 0, 'L' };
            uint8_t on = (cf == F_ACC) ? (f & STEP_ACCENT) : (cf == F_SLIDE) ? (f & STEP_SLIDE) : (f & STEP_LOCK);
            ui_put(x, y, (uint8_t)((on ? sym_letter[cf] : '.') - 32 + 96));
        } else {
            ui_invert(x, y, w);
        }
        ui_invert(1, y, 2);
    }
}

static void draw_all(void)
{
    uint8_t s;
    ui_cls();
    ui_header("GRID");
    for (s = 0; s < 16; s++) draw_step(s);
    last_pat = seq_cur;
    last_commits = evo_commits;
}

void scr_grid_enter(void) BANKED
{
    draw_all();
}

static void edited(void)
{
    app_pattern_edited();
    draw_step(cs);
}

void scr_grid_frame(void) BANKED
{
    pattern_t *p = &proj.pat[seq_cur];
    uint8_t old_s = cs, old_f = cf;
    uint8_t f = p->flags[cs], t = f & STEP_TYPE_MASK;
    int8_t d;

    if (seq_cur != last_pat || evo_commits != last_commits) draw_all();

    if (!(key_held & (J_SELECT | J_B))) {
        if (key_repeat & J_UP) cs = (uint8_t)((cs + 15) & 15);
        if (key_repeat & J_DOWN) cs = (uint8_t)((cs + 1) & 15);
        if (key_repeat & J_LEFT) cf = cf ? (uint8_t)(cf - 1) : (uint8_t)(F_COUNT - 1);
        if (key_repeat & J_RIGHT) cf = (uint8_t)((cf + 1) % F_COUNT);
        if (key_pressed & J_A) {
            switch (cf) {
            case F_NOTE:
                if (t == STEP_REST) { p->flags[cs] = (uint8_t)((f & ~STEP_TYPE_MASK) | STEP_NOTE); app_audition(p->note[cs], f & STEP_ACCENT); }
                else if (t == STEP_NOTE) p->flags[cs] = (uint8_t)((f & ~(STEP_TYPE_MASK | STEP_SLIDE | STEP_ACCENT)) | STEP_TIE);
                else p->flags[cs] = (uint8_t)(f & ~(STEP_TYPE_MASK | STEP_SLIDE | STEP_ACCENT));
                break;
            case F_ACC:   p->flags[cs] ^= STEP_ACCENT; break;
            case F_SLIDE: p->flags[cs] ^= STEP_SLIDE; break;
            case F_PROB:  p->prob[cs] = (uint8_t)(p->prob[cs] | 7); break;
            case F_COND:  p->prob[cs] &= 0x0F; break;
            default:      p->flags[cs] ^= STEP_LOCK; break;
            }
            edited();
        }
    } else if (key_held & J_B) {
        d = ui_adjust(12);
        if (d) {
            switch (cf) {
            case F_NOTE:
                p->note[cs] = clampadd(p->note[cs], d, NOTE_MIN, NOTE_MAX);
                if (t != STEP_NOTE) p->flags[cs] = (uint8_t)((f & ~STEP_TYPE_MASK) | STEP_NOTE);
                app_audition(p->note[cs], p->flags[cs] & STEP_ACCENT);
                break;
            case F_PROB:
                p->prob[cs] = (uint8_t)((p->prob[cs] & 0xF0) | clampadd(p->prob[cs] & 7, d > 0 ? 1 : -1, 0, 7));
                break;
            case F_COND: {
                uint8_t c = (uint8_t)(p->prob[cs] >> 4);
                c = (uint8_t)((c + (d > 0 ? 1 : COND_COUNT - 1)) % COND_COUNT);
                p->prob[cs] = (uint8_t)((p->prob[cs] & 0x0F) | (c << 4));
                break;
            }
            case F_ACC:   if (d > 0) p->flags[cs] |= STEP_ACCENT; else p->flags[cs] &= (uint8_t)~STEP_ACCENT; break;
            case F_SLIDE: if (d > 0) p->flags[cs] |= STEP_SLIDE; else p->flags[cs] &= (uint8_t)~STEP_SLIDE; break;
            default:      if (d > 0) p->flags[cs] |= STEP_LOCK; else p->flags[cs] &= (uint8_t)~STEP_LOCK; break;
            }
            edited();
        }
    }
    if (old_s != cs || old_f != cf) {
        draw_step(old_s);
        draw_step(cs);
    }
    if (seq_step != last_step) {
        if (last_step < 16) draw_step(last_step);
        last_step = seq_step;
        draw_step(last_step);
    }
}

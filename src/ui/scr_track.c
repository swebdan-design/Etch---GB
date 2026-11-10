/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * TRACK: tracker-style step editor.  One row per 16th, every channel in view.
 *
 *    # NOT ASL KSHO P CD
 *   >0 A-1 A.. K.H. 8 --
 *
 *   NOT   A          on an empty step: write the last note
 *         A (tap)    on a note: note <-> tie (=== holds the previous note)
 *         A+</>      semitone        A+UP/DOWN   octave
 *   A S L A          accent / slide / lock on-off
 *   KSHO  A          kick / snare / hat / open-hat hit on-off
 *   P     A+DPAD     chance 1..8 (8 = always)
 *   CD    A+DPAD     condition: -- 12 22 14 24 34 44 FI NF
 *   any   B+A        delete / back to default
 *         B+UP/DOWN  jump a beat (4 rows)      B+</>  previous / next pattern
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

enum { F_NOTE, F_ACC, F_SLIDE, F_LOCK, F_K, F_S, F_H, F_O, F_PROB, F_COND, F_COUNT };
static const uint8_t field_x[F_COUNT] = { 3, 7, 8, 9, 11, 12, 13, 14, 16, 18 };
static const uint8_t field_w[F_COUNT] = { 3, 1, 1, 1, 1, 1, 1, 1, 1, 2 };
static const char cond2[COND_COUNT][3] = { "--", "12", "22", "14", "24", "34", "44", "FI", "NF" };
static const char lane_ch[NUM_LANES] = { 'K', 'S', 'H', 'O' };
static const char hex[] = "0123456789ABCDEF";
static const char * const hints[F_COUNT] = {
    "NOTE A:ADD TAP:TIE", "A:ACCENT ON/OFF", "A:SLIDE ON/OFF", "A:LOCK STEP",
    "A:KICK ON/OFF", "A:SNARE ON/OFF", "A:HAT ON/OFF", "A:OPEN HAT ON/OFF",
    "A+DPAD:CHANCE /8", "A+DPAD:CONDITION"
};

static uint8_t cs, cf, edited_here;
static uint8_t last_step = 0xFF, last_pat = 0xFF, last_commits, last_play;

static void draw_step(uint8_t s)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t y = (uint8_t)(2 + s), f = p->flags[s], t = f & STEP_TYPE_MASK, l;
    uint8_t pr = p->prob[s], c = (uint8_t)(pr >> 4);
    uint8_t byte = s >> 3, bit = (uint8_t)(1 << (s & 7));
    ui_put(0, y, (seq_playing && seq_step == s) ? T_PLAY : 0);
    ui_put(1, y, (uint8_t)(hex[s] - 32));
    ui_put(2, y, 0);
    if (t == STEP_NOTE) ui_note(3, y, p->note[s]);
    else if (t == STEP_TIE) ui_text(3, y, "===");
    else ui_text(3, y, "---");
    ui_put(6, y, 0);
    ui_put(7, y, (uint8_t)(((f & STEP_ACCENT) ? 'A' : '.') - 32));
    ui_put(8, y, (uint8_t)(((f & STEP_SLIDE) ? 'S' : '.') - 32));
    ui_put(9, y, (uint8_t)(((f & STEP_LOCK) ? 'L' : '.') - 32));
    ui_put(10, y, 0);
    for (l = 0; l < NUM_LANES; l++) {
        uint8_t on = ((const uint8_t *)&p->drum[l])[byte] & bit;
        ui_put((uint8_t)(11 + l), y, (uint8_t)((on ? lane_ch[l] : '.') - 32));
    }
    ui_put(15, y, 0);
    ui_put(16, y, (uint8_t)('1' - 32 + (pr & 7)));
    ui_put(17, y, 0);
    if (c >= COND_COUNT) c = 0;
    ui_put(18, y, (uint8_t)(cond2[c][0] - 32));
    ui_put(19, y, (uint8_t)(cond2[c][1] - 32));
    if (s == cs) ui_invert(field_x[cf], y, field_w[cf]);
}

static void draw_all(void)
{
    uint8_t s;
    ui_cls();
    ui_header("TRACK");
    ui_row1(" # NOT ASL KSHO P CD");
    for (s = 0; s < 16; s++) draw_step(s);
    last_pat = seq_cur;
    last_commits = evo_commits;
}

void scr_track_enter(void) BANKED
{
    draw_all();
}

void scr_track_frame(void) BANKED
{
    pattern_t *p = &proj.pat[seq_cur];
    uint8_t old_s = cs, old_f = cf, nav, pg, changed = 0;
    uint8_t f = p->flags[cs], t = f & STEP_TYPE_MASK;
    int8_t d;

    if (seq_cur != last_pat || evo_commits != last_commits) { draw_all(); p = &proj.pat[seq_cur]; }

    nav = ui_nav();
    if (nav & J_UP) cs = (uint8_t)((cs + 15) & 15);
    if (nav & J_DOWN) cs = (uint8_t)((cs + 1) & 15);
    if (nav & J_LEFT) cf = cf ? (uint8_t)(cf - 1) : (uint8_t)(F_COUNT - 1);
    if (nav & J_RIGHT) cf = (uint8_t)((cf + 1) % F_COUNT);
    pg = ui_page();
    if (pg & J_UP) cs = (uint8_t)((cs + 12) & 15);
    if (pg & J_DOWN) cs = (uint8_t)((cs + 4) & 15);
    if (pg & (J_LEFT | J_RIGHT)) {
        uint8_t base = seq_queued != 0xFF ? seq_queued : seq_cur;
        seq_queue_pattern((uint8_t)((base + ((pg & J_RIGHT) ? 1 : NUM_PATTERNS - 1)) % NUM_PATTERNS));
        if (proj.chain.song) proj.chain.song = 0;
        if (!seq_playing) app_redraw();
    }

    switch (cf) {
    case F_NOTE:
        if (ui_a_press()) {
            edited_here = 0;
            if (t == STEP_REST) {
                p->note[cs] = app_last_note;
                p->flags[cs] = (uint8_t)((f & ~STEP_TYPE_MASK) | STEP_NOTE);
                app_audition(p->note[cs], f & STEP_ACCENT);
                edited_here = 1;
                changed = 1;
            } else if (t == STEP_NOTE) {
                app_audition(p->note[cs], f & STEP_ACCENT);
            }
        }
        d = ui_edit(12);
        if (d) {
            if ((p->flags[cs] & STEP_TYPE_MASK) != STEP_NOTE)
                p->flags[cs] = (uint8_t)((p->flags[cs] & ~STEP_TYPE_MASK) | STEP_NOTE);
            p->note[cs] = clampadd(p->note[cs], d, NOTE_MIN, NOTE_MAX);
            app_last_note = p->note[cs];
            app_audition(p->note[cs], p->flags[cs] & STEP_ACCENT);
            changed = 1;
        }
        if (ui_a_tap() && !edited_here) {
            t = p->flags[cs] & STEP_TYPE_MASK;
            if (t == STEP_NOTE) p->flags[cs] = (uint8_t)((p->flags[cs] & ~(STEP_TYPE_MASK | STEP_ACCENT | STEP_SLIDE)) | STEP_TIE);
            else if (t == STEP_TIE) p->flags[cs] = (uint8_t)((p->flags[cs] & ~STEP_TYPE_MASK) | STEP_NOTE);
            changed = 1;
        }
        if (ui_clear()) { p->flags[cs] &= (uint8_t)~(STEP_TYPE_MASK | STEP_ACCENT | STEP_SLIDE); changed = 1; }
        break;
    case F_ACC: case F_SLIDE: case F_LOCK: {
        uint8_t m = cf == F_ACC ? STEP_ACCENT : cf == F_SLIDE ? STEP_SLIDE : STEP_LOCK;
        if (ui_a_press()) { ui_a_consume(); p->flags[cs] ^= m; changed = 1; }
        if (ui_clear()) { p->flags[cs] &= (uint8_t)~m; changed = 1; }
        break;
    }
    case F_K: case F_S: case F_H: case F_O: {
        uint8_t l = (uint8_t)(cf - F_K);
        uint16_t bit = (uint16_t)1 << cs;
        if (ui_a_press()) {
            ui_a_consume();
            p->drum[l] ^= bit;
            if (!seq_playing && (p->drum[l] & bit)) drums_play((uint8_t)(1 << l), p->kit);
            changed = 1;
        }
        if (ui_clear()) { p->drum[l] &= (uint16_t)~bit; changed = 1; }
        break;
    }
    case F_PROB:
        d = ui_edit(4);
        if (d) { p->prob[cs] = (uint8_t)((p->prob[cs] & 0xF0) | clampadd(p->prob[cs] & 7, d, 0, 7)); changed = 1; }
        if (ui_clear()) { p->prob[cs] |= 7; changed = 1; }
        break;
    default: {
        uint8_t c = (uint8_t)(p->prob[cs] >> 4);
        d = ui_edit(1);
        if (d) {
            c = (uint8_t)((c + (d > 0 ? 1 : COND_COUNT - 1)) % COND_COUNT);
            p->prob[cs] = (uint8_t)((p->prob[cs] & 0x0F) | (c << 4));
            changed = 1;
        }
        if (ui_clear()) { p->prob[cs] &= 0x0F; changed = 1; }
        break;
    }
    }
    if (changed) { app_pattern_edited(); draw_step(cs); }
    if (old_f != cf) ui_msg(hints[cf]);
    if (old_s != cs || old_f != cf) {
        draw_step(old_s);
        draw_step(cs);
    }
    if (seq_step != last_step || seq_playing != last_play) {
        if (last_step < 16) draw_step(last_step);
        last_step = seq_step;
        last_play = seq_playing;
        draw_step(last_step);
    }
}

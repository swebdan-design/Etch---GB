/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * SONG: tempo, swing, the 16-slot chain (pattern x bars) and pattern tools.
 *
 *   SONG MODE   A toggles (ON = follow the chain, OFF = loop the pattern)
 *   BPM, SWING  </> or A+</> by 1, A+UP/DOWN by 10 (BPM)
 *   slot        D-pad moves; A+</> pattern (or END), A+UP/DOWN bars 1-8,
 *               A tap: song mode -> jump there at the next bar,
 *                      loop mode -> queue that pattern;  B+A: END
 *   COPY        A+DPAD destination, A copies the current pattern there
 *   CLEAR       A twice clears the current pattern (undoable)
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

#define C_SONG  0
#define C_BPM   1
#define C_SWING 2
#define C_SLOT0 3
#define C_COPY  19
#define C_CLEAR 20
#define C_COUNT 21
#define ROW_SLOT 5

static uint8_t cur = C_SLOT0, copy_to = 1, clear_arm;
static uint8_t last_pos = 0xFF, last_pat = 0xFF, last_loop;

static void draw_slot(uint8_t i)
{
    uint8_t x = (uint8_t)((i >> 3) * 10), y = (uint8_t)(ROW_SLOT + (i & 7));
    uint8_t pt = proj.chain.pat[i];
    ui_put(x, y, (proj.chain.song && seq_chain_pos == i) ? T_PLAY : 0);
    ui_num0((uint8_t)(x + 1), y, (uint16_t)(i + 1), 2);
    ui_put((uint8_t)(x + 3), y, 0);
    if (pt < NUM_PATTERNS) {
        ui_put((uint8_t)(x + 4), y, 'P' - 32);
        ui_num0((uint8_t)(x + 5), y, (uint16_t)(pt + 1), 2);
        ui_put((uint8_t)(x + 7), y, 0);
        ui_put((uint8_t)(x + 8), y, 'x' - 32);
        ui_put((uint8_t)(x + 9), y, (uint8_t)('0' - 32 + proj.chain.rep[i]));
    } else {
        ui_textn((uint8_t)(x + 4), y, "END   ", 6);
    }
    if (cur == C_SLOT0 + i) ui_invert((uint8_t)(x + 1), y, 9);
}

static void draw_misc(void)
{
    ui_text(0, 2, "SONG MODE  ");
    ui_textn(11, 2, proj.chain.song ? "ON  CHAIN" : "OFF LOOP ", 9);
    if (cur == C_SONG) ui_invert(0, 2, 9);
    ui_text(0, 3, "BPM        ");
    ui_num(11, 3, proj.synth.bpm, 3);
    ui_fill(14, 3, 6, 0);
    if (cur == C_BPM) ui_invert(0, 3, 3);
    ui_text(0, 4, "SWING      ");
    ui_num(11, 4, proj.synth.swing, 1);
    ui_put(12, 4, 0);
    ui_meter(13, 4, 4, proj.synth.swing, 8);
    if (cur == C_SWING) ui_invert(0, 4, 5);
    ui_fill(0, 13, SCR_W, 0);
    ui_text(0, 14, "COPY P");
    ui_num0(6, 14, (uint16_t)(seq_cur + 1), 2);
    ui_text(8, 14, " TO P");
    ui_num0(13, 14, (uint16_t)(copy_to + 1), 2);
    ui_fill(15, 14, 5, 0);
    if (cur == C_COPY) ui_invert(0, 14, 15);
    ui_text(0, 15, "CLEAR P");
    ui_num0(7, 15, (uint16_t)(seq_cur + 1), 2);
    ui_textn(9, 15, clear_arm ? " A AGAIN!" : "         ", 11);
    if (cur == C_CLEAR) ui_invert(0, 15, 9);
    ui_text(0, 16, "PLAYING P");
    ui_num0(9, 16, (uint16_t)(seq_cur + 1), 2);
    ui_text(11, 16, " BAR");
    ui_num(15, 16, (uint16_t)(seq_loop + 1), 3);
    ui_fill(18, 16, 2, 0);
    if (cur >= C_SLOT0 && cur < C_COPY) ui_text(0, 17, "A+</>:PAT A+^v:BARS ");
    else ui_text(0, 17, "A+DPAD:SET  A:DO IT ");
}

static void draw_body(void)
{
    uint8_t i;
    for (i = 0; i < CHAIN_LEN; i++) draw_slot(i);
    draw_misc();
}

void scr_song_enter(void) BANKED
{
    clear_arm = 0;
    copy_to = (uint8_t)((seq_cur + 1) % NUM_PATTERNS);
    ui_cls();
    ui_header("SONG");
    draw_body();
}

void scr_song_frame(void) BANKED
{
    uint8_t old = cur, redraw = 0, nav;
    int8_t d;

    nav = ui_nav();
    if (nav & J_UP) {
        if (cur >= C_SLOT0 && cur < C_COPY && ((cur - C_SLOT0) & 7) == 0) cur = C_SWING;
        else if (cur == C_COPY) cur = (uint8_t)(C_SLOT0 + 7);
        else if (cur) cur--;
        else cur = C_CLEAR;
    }
    if (nav & J_DOWN) {
        if (cur >= C_SLOT0 && cur < C_COPY && ((cur - C_SLOT0) & 7) == 7) cur = C_COPY;
        else cur = (uint8_t)((cur + 1) % C_COUNT);
    }
    d = 0;
    if (nav & (J_LEFT | J_RIGHT)) {
        if (cur >= C_SLOT0 && cur < C_COPY) cur = (uint8_t)(C_SLOT0 + ((cur - C_SLOT0) ^ 8));
        else d = (nav & J_RIGHT) ? 1 : -1;            /* plain </> adjusts values */
    }
    if (!d) d = ui_edit(10);

    if (d) {
        if (cur == C_BPM) {
            proj.synth.bpm = clampadd(proj.synth.bpm, d, 60, 220);
            seq_set_tempo();
        } else if (cur == C_SWING) {
            proj.synth.swing = clampadd(proj.synth.swing, d > 0 ? 1 : -1, 0, 8);
            seq_set_tempo();
        } else if (cur >= C_SLOT0 && cur < C_COPY) {
            uint8_t i = (uint8_t)(cur - C_SLOT0);
            if (d == 1 || d == -1) {
                /* END(0xFF) <-> 1..32 */
                uint8_t v = proj.chain.pat[i] < NUM_PATTERNS ? (uint8_t)(proj.chain.pat[i] + 1) : 0;
                v = (uint8_t)((v + (d > 0 ? 1 : NUM_PATTERNS)) % (NUM_PATTERNS + 1));
                proj.chain.pat[i] = v ? (uint8_t)(v - 1) : 0xFF;
                if (!proj.chain.rep[i]) proj.chain.rep[i] = 1;
            } else {
                proj.chain.rep[i] = clampadd(proj.chain.rep[i], d > 0 ? 1 : -1, 1, 8);
            }
        } else if (cur == C_COPY) {
            int8_t s = (d == 1 || d == -1) ? d : (d > 0 ? 8 : -8);
            copy_to = (uint8_t)((copy_to + NUM_PATTERNS + s) % NUM_PATTERNS);
        } else if (cur == C_SONG) {
            proj.chain.song = d > 0;
        }
        redraw = 1;
    }
    if (ui_clear() && cur >= C_SLOT0 && cur < C_COPY) {
        proj.chain.pat[cur - C_SLOT0] = 0xFF;
        redraw = 1;
    }
    if (ui_a_tap()) {
        if (cur == C_SONG) {
            proj.chain.song ^= 1;
            if (proj.chain.song) seq_jump_chain(0);
            ui_msg(proj.chain.song ? "SONG MODE ON" : "LOOP MODE");
        } else if (cur >= C_SLOT0 && cur < C_COPY) {
            uint8_t i = (uint8_t)(cur - C_SLOT0);
            if (proj.chain.pat[i] < NUM_PATTERNS) {
                if (proj.chain.song) seq_jump_chain(i);
                else seq_queue_pattern(proj.chain.pat[i]);
                ui_msg("QUEUED FOR NEXT BAR");
            }
        } else if (cur == C_COPY) {
            if (copy_to != seq_cur) {
                CRITICAL {
                    memcpy(&proj.pat[copy_to], &proj.pat[seq_cur], sizeof(pattern_t));
                }
                ui_msg("PATTERN COPIED");
            }
        } else if (cur == C_CLEAR) {
            if (clear_arm) {
                seq_backup_for_undo(seq_cur);
                pattern_clear(&proj.pat[seq_cur]);
                app_pattern_edited();
                clear_arm = 0;
                ui_msg("PATTERN CLEARED");
            } else {
                clear_arm = 1;
            }
        }
        redraw = 1;
    }
    if (old != cur) { clear_arm = 0; redraw = 1; }
    if (seq_chain_pos != last_pos || seq_cur != last_pat || seq_loop != last_loop) {
        last_pos = seq_chain_pos;
        last_pat = seq_cur;
        last_loop = seq_loop;
        redraw = 1;
    }
    if (redraw) draw_body();
}

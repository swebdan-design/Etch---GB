/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * CHAIN: song arrangement (16 slots: pattern x bars) and pattern tools.
 *
 *   SONG row         A toggles song mode (follow the chain)
 *   slot             D-pad moves; B+UP/DOWN pattern (or END), B+LEFT/RIGHT bars
 *                    A: song mode -> jump there at the next bar
 *                       loop mode -> queue that slot's pattern
 *   COPY row         B+UP/DOWN choose destination, A copies the current pattern
 *   CLEAR row        A twice clears the current pattern
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

#define C_SONG  0
#define C_SLOT0 1
#define C_COPY  17
#define C_CLEAR 18
#define C_COUNT 19

static uint8_t cur = C_SLOT0, copy_to = 1, clear_arm;
static uint8_t last_pos = 0xFF, last_pat = 0xFF;

static void draw_slot(uint8_t i)
{
    uint8_t x = (uint8_t)((i >> 3) * 10), y = (uint8_t)(3 + (i & 7));
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
    ui_textn(11, 2, proj.chain.song ? "ON " : "OFF", 3);
    ui_fill(14, 2, 6, 0);
    if (cur == C_SONG) ui_invert(0, 2, 9);
    ui_text(0, 12, "COPY P");
    ui_num0(6, 12, (uint16_t)(seq_cur + 1), 2);
    ui_text(8, 12, " TO P");
    ui_num0(13, 12, (uint16_t)(copy_to + 1), 2);
    ui_fill(15, 12, 5, 0);
    if (cur == C_COPY) ui_invert(0, 12, 15);
    ui_text(0, 13, "CLEAR P");
    ui_num0(7, 13, (uint16_t)(seq_cur + 1), 2);
    ui_textn(9, 13, clear_arm ? " A AGAIN!" : "         ", 11);
    if (cur == C_CLEAR) ui_invert(0, 13, 9);
    ui_text(0, 15, "PLAYING P");
    ui_num0(9, 15, (uint16_t)(seq_cur + 1), 2);
    ui_text(11, 15, " BAR");
    ui_num(15, 15, (uint16_t)(seq_loop + 1), 3);
    ui_text(0, 16, "B+DPAD:EDIT A:GO    ");
    ui_text(0, 17, "SEL+UP/DN: QUEUE PAT");
}

static void draw_body(void)
{
    uint8_t i;
    for (i = 0; i < CHAIN_LEN; i++) draw_slot(i);
    draw_misc();
}

static void draw_all(void)
{
    ui_cls();
    ui_header("CHAIN");
    draw_body();
}

void scr_chain_enter(void) BANKED
{
    clear_arm = 0;
    copy_to = (uint8_t)((seq_cur + 1) % NUM_PATTERNS);
    draw_all();
}

void scr_chain_frame(void) BANKED
{
    uint8_t old = cur, redraw = 0;
    int8_t d;

    if (!(key_held & J_SELECT)) {
        if (key_held & J_B) {
            d = ui_adjust(1);
            if (d && cur >= C_SLOT0 && cur < C_COPY) {
                uint8_t i = (uint8_t)(cur - C_SLOT0);
                if (key_repeat & (J_UP | J_DOWN)) {
                    /* END(0xFF) <-> 1..32 */
                    uint8_t v = proj.chain.pat[i] < NUM_PATTERNS ? (uint8_t)(proj.chain.pat[i] + 1) : 0;
                    v = (uint8_t)((v + (d > 0 ? 1 : NUM_PATTERNS)) % (NUM_PATTERNS + 1));
                    proj.chain.pat[i] = v ? (uint8_t)(v - 1) : 0xFF;
                    if (!proj.chain.rep[i]) proj.chain.rep[i] = 1;
                } else {
                    proj.chain.rep[i] = clampadd(proj.chain.rep[i], d > 0 ? 1 : -1, 1, 8);
                }
                redraw = 1;
            } else if (d && cur == C_COPY) {
                copy_to = (uint8_t)((copy_to + (d > 0 ? 1 : NUM_PATTERNS - 1)) % NUM_PATTERNS);
                redraw = 1;
            }
        } else {
            if (key_repeat & J_UP) {
                if (cur >= C_SLOT0 && cur < C_COPY && ((cur - C_SLOT0) & 7) == 0) cur = C_SONG;
                else if (cur == C_COPY) cur = (uint8_t)(C_SLOT0 + 7);
                else if (cur) cur--;
                else cur = C_CLEAR;
            }
            if (key_repeat & J_DOWN) {
                if (cur >= C_SLOT0 && cur < C_COPY && ((cur - C_SLOT0) & 7) == 7) cur = C_COPY;
                else cur = (uint8_t)((cur + 1) % C_COUNT);
            }
            if ((key_repeat & (J_LEFT | J_RIGHT)) && cur >= C_SLOT0 && cur < C_COPY)
                cur = (uint8_t)(C_SLOT0 + ((cur - C_SLOT0) ^ 8));
            if (key_pressed & J_A) {
                if (cur == C_SONG) {
                    proj.chain.song ^= 1;
                    if (proj.chain.song) seq_jump_chain(0);
                    ui_msg(proj.chain.song ? "SONG MODE ON" : "LOOP MODE");
                } else if (cur < C_COPY) {
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
        }
    }
    if (old != cur) { clear_arm = 0; redraw = 1; }
    if (seq_chain_pos != last_pos || seq_cur != last_pat) {
        last_pos = seq_chain_pos;
        last_pat = seq_cur;
        redraw = 1;
    }
    if (redraw) draw_body();
    else {
        ui_num(15, 15, (uint16_t)(seq_loop + 1), 3);
    }
}

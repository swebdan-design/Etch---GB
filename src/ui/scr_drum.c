/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * DRUM: four 16-step percussion lanes, kits, mutes, locks, groove presets.
 *
 *   grid (rows 3-6):  D-pad moves, A toggles a hit
 *   lane rows 9-12:   LEFT/RIGHT select KIT / MUTE / LOCK
 *                     KIT: B+UP/DOWN change sound, A audition
 *                     MUTE, LOCK: A toggles
 *   GROOVE row 14:    B+UP/DOWN choose style, A applies it (unlocked lanes)
 *   GEN DRUMS row 15: A generates drums from the GEN settings
 *   CLEAR row 16:     A clears unlocked lanes
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"
#include "corrosion.h"

#define ROW_GRID 3
#define ROW_SET  9
#define ROW_ACT  14
/* cursor rows: 0..3 grid lanes, 4..7 lane settings, 8..10 actions */
#define CR_GRID  0
#define CR_SET   4
#define CR_ACT   8
#define CR_COUNT 11

static const char * const lane_lbl[NUM_LANES] = { "KK", "SN", "CH", "OH" };
static uint8_t cr, cx, setf, groove;
static uint8_t last_step = 0xFF, last_pat = 0xFF, last_commits;

static void draw_grid(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t l, s;
    for (s = 0; s < 16; s++)
        ui_put((uint8_t)(3 + s), 2, (seq_playing && s == seq_step) ? T_PLAYHEAD : ((s & 3) ? 0 : T_DOT));
    for (l = 0; l < NUM_LANES; l++) {
        uint8_t y = (uint8_t)(ROW_GRID + l);
        ui_text(0, y, lane_lbl[l]);
        if (cr == CR_GRID + l) ui_invert(0, y, 2);
        ui_put(2, y, (p->lock & (PLOCK_LANE0 << l)) ? T_LOCK : ((proj.mutes & (2 << l)) ? T_MUTE : 0));
        for (s = 0; s < 16; s++) {
            uint8_t on = (p->drum[l] >> s) & 1;
            uint8_t tile = on ? T_CELL_ON : ((s & 3) ? T_CELL_OFF : T_CELL_BEAT);
            if (cr == CR_GRID + l && cx == s) tile = on ? T_CELL_CUR_ON : T_CELL_CUR_OFF;
            ui_put((uint8_t)(3 + s), y, tile);
        }
    }
}

static void draw_settings(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t l;
    ui_text(0, 8, "LN KIT   MUTE LOCK  ");
    for (l = 0; l < NUM_LANES; l++) {
        uint8_t y = (uint8_t)(ROW_SET + l);
        ui_text(0, y, lane_lbl[l]);
        ui_put(2, y, 0);
        ui_textn(3, y, drum_kit_names[l][p->kit[l] & 3], 5);
        ui_fill(8, y, 2, 0);
        ui_textn(10, y, (proj.mutes & (2 << l)) ? "MUT" : "---", 3);
        ui_fill(13, y, 2, 0);
        ui_textn(15, y, (p->lock & (PLOCK_LANE0 << l)) ? "LCK" : "---", 3);
        ui_fill(18, y, 2, 0);
        if (cr == CR_SET + l) {
            static const uint8_t fx[3] = { 3, 10, 15 };
            static const uint8_t fw[3] = { 5, 3, 3 };
            ui_invert(fx[setf], y, fw[setf]);
        }
    }
    ui_fill(0, 13, SCR_W, 0);
    ui_text(0, ROW_ACT, "GROOVE ");
    ui_textn(7, ROW_ACT, corr_style_names[groove], 10);
    ui_fill(17, ROW_ACT, 3, 0);
    ui_text(0, ROW_ACT + 1, "GENERATE DRUMS      ");
    ui_text(0, ROW_ACT + 2, "CLEAR DRUMS         ");
    if (cr >= CR_ACT) ui_invert(0, (uint8_t)(ROW_ACT + cr - CR_ACT), cr == CR_ACT ? 6 : 14);
    ui_text(0, 17, "A:SET  B+DPAD:CHANGE");
}

static void draw_all(void)
{
    ui_cls();
    ui_header("DRUM");
    draw_grid();
    draw_settings();
    last_pat = seq_cur;
    last_commits = evo_commits;
}

void scr_drum_enter(void) BANKED
{
    groove = proj.pat[seq_cur].style % STYLE_COUNT;
    draw_all();
}

void scr_drum_frame(void) BANKED
{
    pattern_t *p = &proj.pat[seq_cur];
    uint8_t redraw = 0;
    int8_t d;

    if (seq_cur != last_pat || evo_commits != last_commits) {
        draw_all();
    }
    if (!(key_held & (J_SELECT | J_B))) {
        if (key_repeat & J_UP) { cr = cr ? (uint8_t)(cr - 1) : (uint8_t)(CR_COUNT - 1); redraw = 1; }
        if (key_repeat & J_DOWN) { cr = (uint8_t)((cr + 1) % CR_COUNT); redraw = 1; }
        if (key_repeat & (J_LEFT | J_RIGHT)) {
            int8_t dir = (key_repeat & J_RIGHT) ? 1 : -1;
            if (cr < CR_SET) cx = (uint8_t)((cx + 16 + dir) & 15);
            else if (cr < CR_ACT) setf = (uint8_t)((setf + 3 + dir) % 3);
            redraw = 1;
        }
        if (key_pressed & J_A) {
            if (cr < CR_SET) {
                p->drum[cr] ^= (uint16_t)1 << cx;
                if (p->drum[cr] & ((uint16_t)1 << cx)) {
                    uint8_t lanes = (uint8_t)(1 << cr);
                    if (!seq_playing) drums_play(lanes, p->kit);
                }
                app_pattern_edited();
            } else if (cr < CR_ACT) {
                uint8_t l = (uint8_t)(cr - CR_SET);
                if (setf == 0) drums_play((uint8_t)(1 << l), p->kit);
                else if (setf == 1) proj.mutes ^= (uint8_t)(2 << l);
                else { p->lock ^= (uint8_t)(PLOCK_LANE0 << l); app_pattern_edited(); }
            } else if (cr == CR_ACT) {
                seq_backup_for_undo(seq_cur);
                corr_drum_preset(p, groove);
                app_pattern_edited();
                ui_msg("GROOVE APPLIED");
            } else if (cr == CR_ACT + 1) {
                seq_backup_for_undo(seq_cur);
                corr_generate(p, &proj.gen, GEN_DRUMS);
                app_pattern_edited();
                ui_msg("DRUMS GENERATED");
            } else {
                uint8_t l;
                seq_backup_for_undo(seq_cur);
                for (l = 0; l < NUM_LANES; l++)
                    if (!(p->lock & (PLOCK_LANE0 << l))) p->drum[l] = 0;
                app_pattern_edited();
                ui_msg("DRUMS CLEARED");
            }
            redraw = 1;
        }
    } else if (key_held & J_B) {
        d = ui_adjust(1);
        if (d) {
            if (cr >= CR_SET && cr < CR_ACT && setf == 0) {
                uint8_t l = (uint8_t)(cr - CR_SET);
                p->kit[l] = (uint8_t)((p->kit[l] + (d > 0 ? 1 : 3)) & 3);
                drums_play((uint8_t)(1 << l), p->kit);
                app_pattern_edited();
            } else if (cr == CR_ACT) {
                groove = (uint8_t)((groove + (d > 0 ? 1 : STYLE_COUNT - 1)) % STYLE_COUNT);
            }
            redraw = 1;
        }
    }
    if (redraw) { draw_grid(); draw_settings(); }
    if (seq_step != last_step) {
        last_step = seq_step;
        draw_grid();
    }
}

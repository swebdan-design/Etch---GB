/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * SAVE: 8 battery-backed project slots + the 8 ROM tracks (the album).
 *
 *   UP/DOWN      select a slot or track
 *   LEFT/RIGHT   slot action: LOAD / SAVE / ERASE
 *   A            arm; A again confirms (saving to an empty slot is immediate)
 *   A, A on a track loads that ROM track
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"
#include "storage.h"
#include "presets.h"

#define N_ROWS (NUM_SLOTS + NUM_TRACKS)
static const char * const act_n[3] = { "LOAD", "SAVE", "ERAS" };

static uint8_t cur, act = 1, armed = 0xFF;
static uint8_t status[NUM_SLOTS];
static char names[NUM_SLOTS][PROJECT_NAME_LEN];
static uint8_t last_slot;

static void scan_one(uint8_t s)
{
    status[s] = storage_slot_status(s, names[s]);
    last_slot = storage_last_slot();
}

static void scan(void)
{
    uint8_t s;
    for (s = 0; s < NUM_SLOTS; s++) status[s] = storage_slot_status(s, names[s]);
    last_slot = storage_last_slot();
}

static void draw_row(uint8_t r)
{
    uint8_t y = (uint8_t)(2 + r);
    ui_fill(0, y, SCR_W, 0);
    if (r < NUM_SLOTS) {
        ui_put(0, y, 'S' - 32);
        ui_put(1, y, (uint8_t)('1' - 32 + r));
        ui_put(2, y, r == last_slot ? ('*' - 32) : 0);
        if (status[r] == SLOT_OK) ui_textn(3, y, names[r], PROJECT_NAME_LEN);
        else if (status[r] == SLOT_BAD) ui_text(3, y, "!CORRUPT!");
        else ui_text(3, y, "--empty--");
        if (cur == r) {
            ui_invert(0, y, 2);
            ui_text(15, y, act_n[act]);
            ui_invert(15, y, 4);
            if (armed == r) ui_put(19, y, '?' - 32 + 96);
        }
    } else {
        uint8_t t = (uint8_t)(r - NUM_SLOTS);
        ui_put(0, y, 'T' - 32);
        ui_put(1, y, (uint8_t)('1' - 32 + t));
        ui_textn(3, y, track_names[t], 14);
        if (cur == r) {
            ui_invert(0, y, 2);
            if (armed == r) { ui_text(17, y, "A?"); ui_invert(17, y, 2); }
        }
    }
}

static void draw_all(void)
{
    uint8_t r;
    ui_cls();
    ui_header("SAVE");
    for (r = 0; r < N_ROWS; r++) draw_row(r);
}

void scr_save_enter(void) BANKED
{
    armed = 0xFF;
    scan();
    draw_all();
}

void scr_save_frame(void) BANKED
{
    uint8_t old = cur, nav = ui_nav();
    int8_t d = ui_edit(1);
    if (nav & J_UP) cur = cur ? (uint8_t)(cur - 1) : (uint8_t)(N_ROWS - 1);
    if (nav & J_DOWN) cur = (uint8_t)((cur + 1) % N_ROWS);
    if (((nav & (J_LEFT | J_RIGHT)) || d) && cur < NUM_SLOTS) {
        act = (uint8_t)((act + (((nav & J_RIGHT) || d > 0) ? 1 : 2)) % 3);
        armed = 0xFF;
        draw_row(cur);
    }
    if (old != cur) {
        armed = 0xFF;
        draw_row(old);
        draw_row(cur);
    }
    if (ui_a_tap()) {
        if (cur < NUM_SLOTS) {
            uint8_t s = cur;
            uint8_t needs_confirm = !(act == 1 && status[s] == SLOT_EMPTY);
            if (act == 0 && status[s] != SLOT_OK) {
                ui_msg(status[s] == SLOT_BAD ? "SLOT CORRUPT" : "SLOT EMPTY");
            } else if (needs_confirm && armed != s) {
                armed = s;
                ui_msg("PRESS A TO CONFIRM");
            } else {
                armed = 0xFF;
                if (act == 0) {
                    if (storage_load(s)) { app_project_loaded(); ui_msg("LOADED"); }
                    else ui_msg("LOAD FAILED");
                } else if (act == 1) {
                    ui_msg(storage_save(s) ? "SAVED + VERIFIED" : "SAVE FAILED");
                } else {
                    storage_erase(s);
                    ui_msg("SLOT ERASED");
                }
                scan_one(s);
                { uint8_t r; for (r = 0; r < NUM_SLOTS; r++) draw_row(r); }
            }
            draw_row(s);
        } else {
            uint8_t t = (uint8_t)(cur - NUM_SLOTS);
            if (armed != cur) {
                armed = cur;
                ui_msg("A AGAIN: LOAD TRACK");
            } else {
                armed = 0xFF;
                app_load_track(t);
                ui_msg("TRACK LOADED");
            }
            draw_row(cur);
        }
    }
}

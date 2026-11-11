/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * FILE: projects, the demo songs, saving, and the ABOUT pages.
 *
 *   menu    NEW     A, A    start an empty project
 *           LOAD    A       open the list of demo songs and your saves
 *           SAVE    A       open the 8 save slots
 *           ABOUT   A       what ETCH is, generative music, Synth Centric
 *   lists   UP/DOWN choose, A arm, A again do it, B back to the menu
 *   SAVE    B+A on a used slot erases it (A again confirms)
 *   ABOUT   LEFT/RIGHT turn the page, B back
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"
#include "storage.h"
#include "presets.h"

enum { V_MENU, V_LOAD, V_SAVE, V_ABOUT };
#define N_MENU 4
#define N_LOAD (NUM_TRACKS + NUM_SLOTS)
#define N_ABOUT 4

static const char * const menu_n[N_MENU] = { "NEW", "LOAD", "SAVE", "ABOUT" };
static const char * const menu_d[N_MENU] = { "empty song", "demos/saves", "8 slots", "the story" };

/* ABOUT pages: 14 lines of up to 20 characters each */
static const char * const about[N_ABOUT][14] = {
    {   "ETCH",
        "A GENERATIVE",
        "ACID MACHINE",
        "",
        "An acid synthesizer,",
        "drum machine, step",
        "sequencer and",
        "generative composer",
        "for the Game Boy.",
        "",
        "It is also an album:",
        "eight songs to play,",
        "reshape, let evolve",
        "and keep as yours." },
    {   "GENERATIVE MUSIC",
        "",
        "Generative music is",
        "made by a system of",
        "rules and chance",
        "instead of being",
        "fixed note by note.",
        "",
        "ETCH's CORROSION",
        "engine writes, then",
        "slowly rewrites its",
        "own patterns. With",
        "EVO on, a song is",
        "never the same twice" },
    {   "SYNTH CENTRIC",
        "",
        "Dan Schwebel, known",
        "as Synth Centric, is",
        "an Australian beat",
        "maker, music",
        "producer, writer and",
        "educator.",
        "",
        "ETCH is a",
        "Synth Centric",
        "release.",
        "",
        "" },
    {   "ETCH v" ETCH_VERSION_STR,
        "by Synth Centric",
        "",
        "Every listen is",
        "different: set EVO",
        "to DRIFT on LIVE and",
        "the song changes a",
        "little every 4 bars.",
        "",
        "Built with GBDK-2020",
        "Thanks: furrtek's",
        "GB-303 (CC BY 4.0)",
        "",
        "(c) Synth Centric" },
};

static uint8_t view, cur, page, armed = 0xFF, arm_erase;
static uint8_t status[NUM_SLOTS];
static char names[NUM_SLOTS][PROJECT_NAME_LEN];
static uint8_t last_slot;

static void scan(void)
{
    uint8_t s;
    for (s = 0; s < NUM_SLOTS; s++) status[s] = storage_slot_status(s, names[s]);
    last_slot = storage_last_slot();
}

static void clear_body(void)
{
    uint8_t y;
    for (y = 2; y < SCR_H; y++) ui_fill(0, y, SCR_W, 0);
}

static void slot_line(uint8_t y, uint8_t s)
{
    ui_put(0, y, 'S' - 32);
    ui_put(1, y, (uint8_t)('1' - 32 + s));
    ui_put(2, y, s == last_slot ? ('*' - 32) : 0);
    if (status[s] == SLOT_OK) ui_textn(3, y, names[s], PROJECT_NAME_LEN);
    else if (status[s] == SLOT_BAD) ui_text(3, y, "!CORRUPT!");
    else ui_text(3, y, "--empty--");
}

static void draw_menu(void)
{
    uint8_t i, y;
    ui_row1(0);
    clear_body();
    ui_text(0, 2, "PROJECT");
    ui_textn(8, 2, proj.name, PROJECT_NAME_LEN);
    for (i = 0; i < N_MENU; i++) {
        y = (uint8_t)(4 + i * 2);
        ui_put(0, y, cur == i ? T_ARROW_R : 0);
        ui_textn(1, y, menu_n[i], 5);
        if (cur == i) ui_invert(1, y, 5);
        ui_text(7, y, menu_d[i]);
    }
    if (armed == 0) ui_text(1, 13, "A AGAIN: NEW SONG");
    ui_text(0, 15, "ETCH v" ETCH_VERSION_STR);
    ui_text(0, 16, "by Synth Centric");
    ui_text(0, 17, "A:OPEN");
}

static void draw_load(void)
{
    uint8_t i, y;
    ui_row1("LOAD  DEMOS / SAVES ");
    clear_body();
    for (i = 0; i < N_LOAD; i++) {
        y = (uint8_t)(2 + i);
        if (i < NUM_TRACKS) {
            ui_put(0, y, 'D' - 32);
            ui_put(1, y, (uint8_t)('1' - 32 + i));
            ui_textn(3, y, track_names[i], 14);
        } else {
            slot_line(y, (uint8_t)(i - NUM_TRACKS));
        }
        if (cur == i) {
            ui_invert(0, y, 2);
            if (armed == i) { ui_text(16, y, "A?"); ui_invert(16, y, 2); }
        }
    }
}

static void draw_save(void)
{
    uint8_t s, y;
    ui_row1("SAVE  B+A:ERASE     ");
    clear_body();
    for (s = 0; s < NUM_SLOTS; s++) {
        y = (uint8_t)(2 + s);
        slot_line(y, s);
        if (cur == s) {
            ui_invert(0, y, 2);
            if (armed == s) {
                ui_text(14, y, arm_erase ? "ERASE?" : "OVER?");
                ui_invert(14, y, arm_erase ? 6 : 5);
            }
        }
    }
    ui_text(0, 11, "SAVING");
    ui_textn(7, 11, proj.name, PROJECT_NAME_LEN);
    ui_text(0, 13, "A:SAVE  A,A:REPLACE");
    ui_text(0, 14, "B+A,A:ERASE");
    ui_text(0, 17, "B:BACK");
}

static void draw_about(void)
{
    uint8_t i;
    ui_row1(0);
    clear_body();
    for (i = 0; i < 14; i++) ui_text(0, (uint8_t)(2 + i), about[page][i]);
    ui_invert(0, 2, (uint8_t)strlen(about[page][0]));
    ui_put(0, 17, page ? T_ARROW_L : 0);
    ui_put(1, 17, (uint8_t)('1' - 32 + page));
    ui_put(2, 17, '/' - 32);
    ui_put(3, 17, (uint8_t)('0' - 32 + N_ABOUT));
    ui_put(4, 17, page + 1 < N_ABOUT ? T_ARROW_R : 0);
    ui_text(14, 17, "B:BACK");
}

static void draw(void)
{
    switch (view) {
    case V_LOAD: draw_load(); break;
    case V_SAVE: draw_save(); break;
    case V_ABOUT: draw_about(); break;
    default: draw_menu(); break;
    }
}

void scr_file_enter(void) BANKED
{
    armed = 0xFF;
    ui_cls();
    ui_header("FILE");
    if (view == V_LOAD || view == V_SAVE) scan();
    draw();
}

static void open_view(uint8_t v)
{
    view = v;
    cur = 0;
    page = 0;
    armed = 0xFF;
    if (v == V_LOAD || v == V_SAVE) scan();
    draw();
}

void scr_file_frame(void) BANKED
{
    uint8_t nav = ui_nav(), n, old = cur;

    if (view != V_MENU && ui_b_tap()) {
        uint8_t back = view == V_LOAD ? 2 : view == V_SAVE ? 3 : view == V_ABOUT ? 4 : 0;
        view = V_MENU;
        cur = (uint8_t)(back ? back - 1 : 0);
        armed = 0xFF;
        draw();
        return;
    }

    if (view == V_ABOUT) {
        if ((nav & J_RIGHT) && page + 1 < N_ABOUT) { page++; draw(); }
        if ((nav & J_LEFT) && page) { page--; draw(); }
        if (ui_a_tap() && page + 1 < N_ABOUT) { page++; draw(); }
        return;
    }

    n = view == V_MENU ? N_MENU : view == V_LOAD ? N_LOAD : NUM_SLOTS;
    if (nav & J_UP) cur = cur ? (uint8_t)(cur - 1) : (uint8_t)(n - 1);
    if (nav & J_DOWN) cur = (uint8_t)((cur + 1) % n);
    if (old != cur) { armed = 0xFF; draw(); }

    if (view == V_MENU) {
        if (ui_a_tap()) {
            if (cur == 0) {
                if (armed == 0) {
                    armed = 0xFF;
                    app_new_project();
                    ui_msg("NEW PROJECT");
                    app_go_live(0);
                    return;
                }
                armed = 0;
                ui_msg("A AGAIN: NEW SONG");
            } else {
                open_view(cur);          /* 1 LOAD, 2 SAVE, 3 ABOUT */
                return;
            }
            draw();
        }
        return;
    }

    if (view == V_LOAD) {
        if (ui_a_tap()) {
            if (cur >= NUM_TRACKS && status[cur - NUM_TRACKS] != SLOT_OK) {
                ui_msg(status[cur - NUM_TRACKS] == SLOT_BAD ? "SLOT CORRUPT" : "SLOT EMPTY");
            } else if (armed != cur) {
                armed = cur;
                draw();
                ui_msg(cur < NUM_TRACKS ? "A AGAIN: PLAY DEMO" : "A AGAIN: LOAD SAVE");
            } else {
                armed = 0xFF;
                if (cur < NUM_TRACKS) {
                    app_load_track(cur);
                    ui_msg("DEMO LOADED");
                } else if (storage_load((uint8_t)(cur - NUM_TRACKS))) {
                    app_project_loaded();
                    ui_msg("LOADED");
                } else {
                    draw();
                    ui_msg("LOAD FAILED");
                    return;
                }
                view = V_MENU;
                cur = 1;
                app_go_live(1);
            }
        }
        return;
    }

    /* V_SAVE */
    if (ui_clear()) {
        if (status[cur] == SLOT_EMPTY) { ui_msg("SLOT ALREADY EMPTY"); return; }
        armed = cur;
        arm_erase = 1;
        draw();
        ui_msg("A AGAIN: ERASE");
        return;
    }
    if (ui_a_tap()) {
        const char *msg;
        if (armed == cur && arm_erase) {
            storage_erase(cur);
            msg = "SLOT ERASED";
        } else if (status[cur] != SLOT_EMPTY && armed != cur) {
            armed = cur;
            arm_erase = 0;
            draw();
            ui_msg("A AGAIN: REPLACE");
            return;
        } else {
            msg = storage_save(cur) ? "SAVED + VERIFIED" : "SAVE FAILED";
        }
        armed = 0xFF;
        status[cur] = storage_slot_status(cur, names[cur]);
        last_slot = storage_last_slot();
        draw();
        ui_msg(msg);
    }
}

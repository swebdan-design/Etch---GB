/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Application: boot, main loop, global controls, CORROSION scheduling.
 *
 * The timer interrupt owns all sound and timing.  The main loop (60 Hz) reads
 * the buttons, runs the current screen, flushes changed tile rows, and
 * prepares the next CORROSION mutation in the background so that the
 * interrupt only has to swap it in on a phrase boundary.
 */
#include <gbdk/platform.h>
#include "engine.h"
#include "ui.h"
#include "corrosion.h"
#include "presets.h"

uint8_t app_screen;
static uint8_t need_redraw;
static uint8_t audition_timer;
static pattern_t anchor;            /* DRIFT's musical memory */
static uint8_t anchor_pat = 0xFF;
static uint8_t anchor_dirty;
static pattern_t evo_tmp;
static uint16_t evo_ctr;
static uint8_t last_commits;
static char msgbuf[21];
volatile uint16_t main_loops;
volatile uint8_t main_worst;   /* longest main-loop iteration, frames */

void app_redraw(void) { need_redraw = 1; }

void app_pattern_edited(void)
{
    seq_pattern_edited();
    anchor_dirty = 1;
}

void app_audition(uint8_t note, uint8_t accent)
{
    if (!seq_playing && !(proj.mutes & 1)) {
        synth_note_on(note, accent);
        audition_timer = 16;
    }
}

void app_fill(void)
{
    uint16_t m[NUM_LANES];
    corr_fill(&proj.pat[seq_cur], (uint16_t)(sys_time ^ ((uint16_t)DIV_REG << 8)), m);
    seq_trigger_fill(m);
    ui_msg("FILL");
}

/* Prepare the next mutation of the playing pattern (main-loop context). */
static void evo_service(void)
{
    uint8_t mode = proj.gen.mode, target;
    if (evo_state == EVO_READY && evo_target != seq_cur) {
        CRITICAL { if (evo_state == EVO_READY) evo_state = EVO_IDLE; }   /* playback moved on */
    }
    if (evo_state != EVO_IDLE) return;
    if (mode == EVO_STABLE && !evo_request) return;
    target = seq_cur;
    if (anchor_pat != target || anchor_dirty) {
        memcpy(&anchor, &proj.pat[target], sizeof(pattern_t));
        anchor_pat = target;
        anchor_dirty = 0;
    }
    corr_mutate(&evo_tmp, &proj.pat[target], &anchor, &proj.gen,
                mode == EVO_STABLE ? EVO_DRIFT : mode, proj.gen.mutation,
                (uint16_t)(proj.gen.seed + (++evo_ctr)));
    CRITICAL {
        if (evo_state == EVO_IDLE && seq_cur == target) {
            memcpy(&evo_cand, &evo_tmp, sizeof(pattern_t));
            evo_target = target;
            evo_state = EVO_READY;
        }
    }
}

void app_evolve_now(void)
{
    if (proj.pat[seq_cur].lock & PLOCK_ALL) { ui_msg("PATTERN LOCKED"); return; }
    evo_request = 1;
    if (seq_playing) {
        ui_msg("EVOLVE AT NEXT BAR");
        return;
    }
    /* stopped: mutate and commit immediately */
    evo_state = EVO_IDLE;
    evo_service();
    CRITICAL {
        if (evo_state == EVO_READY && evo_target == seq_cur) {
            memcpy(&evo_undo, &proj.pat[seq_cur], sizeof(pattern_t));
            memcpy(&proj.pat[seq_cur], &evo_cand, sizeof(pattern_t));
            evo_undo_pat = seq_cur;
            evo_state = EVO_IDLE;
            evo_request = 0;
            evo_commits++;
        }
    }
}

void app_undo(void)
{
    if (seq_undo()) {
        ui_msg("UNDO: PREVIOUS GEN");
        app_redraw();
    } else {
        ui_msg("NOTHING TO UNDO");
    }
}

void app_project_loaded(void)
{
    CRITICAL {
        seq_cur = proj.start_pattern < NUM_PATTERNS ? proj.start_pattern : 0;
        seq_queued = 0xFF;
        seq_chain_pos = 0;
        seq_chain_bar = 0;
        if (proj.chain.song && proj.chain.pat[0] < NUM_PATTERNS) seq_cur = proj.chain.pat[0];
        evo_state = EVO_IDLE;
        evo_request = 0;
        evo_undo_pat = 0xFF;
    }
    anchor_pat = 0xFF;
    seq_set_tempo();
    app_redraw();
}

void app_load_track(uint8_t t)
{
    uint8_t was = seq_playing;
    seq_stop();
    preset_load(t);
    app_project_loaded();
    if (was) seq_start();
}

static void enter_screen(void)
{
    switch (app_screen) {
    case SCR_LIVE:  scr_live_enter(); break;
    case SCR_GRID:  scr_grid_enter(); break;
    case SCR_DRUM:  scr_drum_enter(); break;
    case SCR_SYNTH: scr_synth_enter(); break;
    case SCR_GEN:   scr_gen_enter(); break;
    case SCR_CHAIN: scr_chain_enter(); break;
    default:        scr_save_enter(); break;
    }
}

static void frame_screen(void)
{
    switch (app_screen) {
    case SCR_LIVE:  scr_live_frame(); break;
    case SCR_GRID:  scr_grid_frame(); break;
    case SCR_DRUM:  scr_drum_frame(); break;
    case SCR_SYNTH: scr_synth_frame(); break;
    case SCR_GEN:   scr_gen_frame(); break;
    case SCR_CHAIN: scr_chain_frame(); break;
    default:        scr_save_frame(); break;
    }
}

static void global_keys(void)
{
    if (key_held & J_SELECT) {
        if (key_pressed & J_RIGHT) { app_screen = (uint8_t)((app_screen + 1) % SCR_COUNT); need_redraw = 1; }
        if (key_pressed & J_LEFT) { app_screen = (uint8_t)((app_screen + SCR_COUNT - 1) % SCR_COUNT); need_redraw = 1; }
        if (key_pressed & J_UP) {
            uint8_t base = seq_queued != 0xFF ? seq_queued : seq_cur;
            seq_queue_pattern((uint8_t)((base + 1) % NUM_PATTERNS));
            if (proj.chain.song) { proj.chain.song = 0; ui_msg("LOOP MODE: QUEUED"); }
            if (!seq_playing) need_redraw = 1;
        }
        if (key_pressed & J_DOWN) {
            uint8_t base = seq_queued != 0xFF ? seq_queued : seq_cur;
            seq_queue_pattern((uint8_t)((base + NUM_PATTERNS - 1) % NUM_PATTERNS));
            if (proj.chain.song) { proj.chain.song = 0; ui_msg("LOOP MODE: QUEUED"); }
            if (!seq_playing) need_redraw = 1;
        }
        if (key_pressed & J_A) app_evolve_now();
        if (key_pressed & J_B) app_undo();
    } else if (key_pressed & J_START) {
        if (seq_playing) seq_stop();
        else seq_start();
    }
}

void main(void)
{
    DISPLAY_OFF;
    NR52_REG = 0x80;                 /* sound on */
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
    BGP_REG = 0xE4;
    LCDC_REG |= LCDCF_BG8000;
    gfx_load_ui_tiles();
    preset_load(0);
    synth_init();
    drums_init();
    seq_init();
    ui_input_init();
    app_project_loaded();
    SHOW_BKG;
    enable_interrupts();

    scr_title_run();

    app_screen = SCR_LIVE;
    enter_screen();
    ui_flush_all();
    need_redraw = 0;
    DISPLAY_ON;
    seq_start();                     /* press START, hear acid */
    last_commits = evo_commits;

    while (1) {
        uint8_t t0;
        wait_vbl_done();
        t0 = (uint8_t)sys_time;
        main_loops++;
        ui_flush();
        ui_input();
        global_keys();
        if (need_redraw) {
            need_redraw = 0;
            enter_screen();
        } else {
            frame_screen();
        }
        ui_header_update();
        ui_msg_tick();
        if (audition_timer && !--audition_timer) synth_gate_off();
        if (evo_commits != last_commits) {
            uint8_t g = proj.pat[seq_cur].gen;
            last_commits = evo_commits;
            strcpy(msgbuf, "EVOLVED  GEN ");
            msgbuf[13] = (char)('0' + (g / 100) % 10);
            msgbuf[14] = (char)('0' + (g / 10) % 10);
            msgbuf[15] = (char)('0' + g % 10);
            msgbuf[16] = 0;
            ui_msg(msgbuf);
        }
        evo_service();
        t0 = (uint8_t)((uint8_t)sys_time - t0);
        if (t0 > main_worst) main_worst = t0;
    }
}

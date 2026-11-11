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
uint8_t app_last_note = 21;         /* A2: what A writes on an empty step */
static uint8_t need_redraw;
static uint8_t audition_timer;
/* DRIFT's musical memory: one anchor per recently evolved pattern, so every
 * pattern of a song keeps breathing around its own original. */
#define N_ANCHORS 8
static pattern_t anchors[N_ANCHORS];
static uint8_t anchor_id[N_ANCHORS] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t anchor_next;
static uint16_t evo_rng;
static pattern_t evo_tmp;
static uint16_t evo_ctr;
static uint8_t last_commits;
static char msgbuf[21];
volatile uint16_t main_loops;
volatile uint8_t main_worst;   /* longest main-loop iteration, frames */

void app_redraw(void) { need_redraw = 1; }

static void anchors_forget(uint8_t pat)
{
    uint8_t i;
    for (i = 0; i < N_ANCHORS; i++)
        if (pat == 0xFF || anchor_id[i] == pat) anchor_id[i] = 0xFF;
}

static pattern_t *anchor_for(uint8_t pat)
{
    uint8_t i;
    for (i = 0; i < N_ANCHORS; i++)
        if (anchor_id[i] == pat) return &anchors[i];
    i = anchor_next;
    anchor_next = (uint8_t)((anchor_next + 1) & (N_ANCHORS - 1));
    anchor_id[i] = pat;
    memcpy(&anchors[i], &proj.pat[pat], sizeof(pattern_t));
    return &anchors[i];
}

/* fresh randomness for every evolution: the entropy pool (button timing,
 * the moment START was pressed) stirred with a xorshift */
static uint16_t evo_random(void)
{
    evo_rng ^= ui_entropy ^ DIV_REG;
    evo_rng ^= evo_rng << 7;
    evo_rng ^= evo_rng >> 9;
    evo_rng ^= evo_rng << 8;
    if (!evo_rng) evo_rng = 0x2601;
    return evo_rng;
}

/* After each committed evolution the SOUND drifts too: the bass cutoff and
 * every drum lane's tuning take a small random step, bounded around the
 * settings you chose (they are live offsets and are never saved). */
static void sound_drift(void)
{
    uint8_t l, r;
    int8_t lim = proj.gen.mode == EVO_CORRODE ? 5 : 2;
    int8_t dlim = proj.gen.mode == EVO_CORRODE ? 2 : 1;
    int8_t v;
    if (proj.gen.mode == EVO_STABLE) return;
    r = (uint8_t)evo_random();
    v = (int8_t)(evo_cut_ofs + (int8_t)((r & 3) == 0 ? -1 : (r & 3) == 1 ? 1 : 0));
    if (v > lim) v = lim;
    if (v < -lim) v = -lim;
    evo_cut_ofs = v;
    for (l = 0; l < NUM_LANES; l++) {
        r >>= 2;
        if (l == 2) r = (uint8_t)(evo_random() >> 3);
        v = (int8_t)(evo_tune_ofs[l] + (int8_t)((r & 3) == 0 ? -1 : (r & 3) == 1 ? 1 : 0));
        if (v > dlim) v = dlim;
        if (v < -dlim) v = -dlim;
        evo_tune_ofs[l] = v;
    }
    drums_revoice();
}

static void sound_drift_reset(void)
{
    uint8_t l;
    evo_cut_ofs = 0;
    for (l = 0; l < NUM_LANES; l++) evo_tune_ofs[l] = 0;
    drums_revoice();
}

void app_pattern_edited(void)
{
    seq_pattern_edited();
    anchors_forget(seq_cur);           /* your edit becomes the new memory */
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
/* Prepare the next mutation (main-loop context).  It is prepared for the
 * pattern that will play after the next bar line, because that is where the
 * ISR commits it -- in song mode the chain usually changes pattern exactly
 * on a phrase boundary. */
static uint8_t evo_service(void)
{
    uint8_t mode = proj.gen.mode, target = seq_playing ? seq_next_bar_pattern() : seq_cur;
    if (evo_state == EVO_READY && evo_target != target) {
        CRITICAL { if (evo_state == EVO_READY) evo_state = EVO_IDLE; }   /* playback moved on */
    }
    if (evo_state != EVO_IDLE) return 0;
    if (mode == EVO_STABLE && !evo_request) return 0;
    corr_mutate(&evo_tmp, &proj.pat[target], anchor_for(target), &proj.gen,
                mode == EVO_STABLE ? EVO_DRIFT : mode, proj.gen.mutation,
                (uint16_t)(evo_random() + (++evo_ctr)));
    CRITICAL {
        if (evo_state == EVO_IDLE) {
            memcpy(&evo_cand, &evo_tmp, sizeof(pattern_t));
            evo_target = target;
            evo_state = EVO_READY;
        }
    }
    return 1;
}

/* REGEN material is composed in the background so the tap is instant */
static pattern_t regen_mat;
static uint8_t regen_ready;
static gen_t regen_for;               /* GEN settings the material was made with */

static uint8_t regen_service(void)
{
    if (regen_ready && !memcmp(&regen_for, &proj.gen, 8)) return 0;   /* key..octave */
    memcpy(&regen_for, &proj.gen, sizeof(gen_t));
    corr_regen_material(&regen_mat, &proj.gen, evo_random());
    regen_ready = 1;
    return 1;
}

uint16_t app_regen(uint8_t what)
{
    uint16_t mask;
    pattern_t *p = &proj.pat[seq_cur];
    if (p->lock & PLOCK_ALL) return 0;
    regen_service();                  /* only slow if nothing was ready */
    seq_backup_for_undo(seq_cur);
    mask = corr_regen_apply(p, &regen_mat, evo_random(), what);
    regen_ready = 0;                  /* fresh material for next time */
    if (mask) app_pattern_edited();
    return mask;
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
    anchors_forget(0xFF);
    evo_cut_ofs = 0;
    { uint8_t l; for (l = 0; l < NUM_LANES; l++) evo_tune_ofs[l] = 0; }
    seq_set_tempo();
    drums_revoice();
    app_redraw();
}

/* FILE > NEW: an empty project with a sensible starting sound */
void app_new_project(void)
{
    static const uint8_t synth_def[16] = { 0, 8, 4, 8, 7, 8, 0, 6, 4, 0, 0, 0, 124, 0, 12, 1 };
    static const char nm[PROJECT_NAME_LEN] = { 'U','N','T','I','T','L','E','D',' ',' ' };
    uint8_t i;
    seq_stop();
    memcpy(proj.name, nm, PROJECT_NAME_LEN);
    memcpy(&proj.synth, synth_def, sizeof(synth_t));
    proj.gen.key = 9; proj.gen.scale = SCALE_MINOR; proj.gen.style = STYLE_ACID;
    proj.gen.density = 9; proj.gen.sync = 5; proj.gen.accent = 8; proj.gen.slide = 7;
    proj.gen.octave = 0; proj.gen.mutation = 5; proj.gen.mode = EVO_STABLE;
    proj.gen.seed = ui_entropy;
    for (i = 0; i < CHAIN_LEN; i++) { proj.chain.pat[i] = 0xFF; proj.chain.rep[i] = 1; }
    proj.chain.pat[0] = 0;
    proj.chain.rep[0] = 4;
    proj.chain.song = 0;
    proj.mutes = 0;
    proj.start_pattern = 0;
    for (i = 0; i < NUM_PATTERNS; i++) {
        pattern_clear(&proj.pat[i]);
        proj.pat[i].kit[1] = 2;                /* clap */
    }
    project_drum_defaults(&proj);
    app_project_loaded();
}

void app_go_live(uint8_t play)
{
    app_screen = SCR_LIVE;
    need_redraw = 1;
    if (play && !seq_playing) { seq_seed_live(ui_entropy); seq_start(); }
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
    case SCR_SYNTH: scr_synth_enter(); break;
    case SCR_TRACK: scr_track_enter(); break;
    case SCR_LIVE:  scr_live_enter(); break;
    case SCR_GEN:   scr_gen_enter(); break;
    case SCR_SONG:  scr_song_enter(); break;
    default:        scr_file_enter(); break;
    }
}

static void frame_screen(void)
{
    switch (app_screen) {
    case SCR_SYNTH: scr_synth_frame(); break;
    case SCR_TRACK: scr_track_frame(); break;
    case SCR_LIVE:  scr_live_frame(); break;
    case SCR_GEN:   scr_gen_frame(); break;
    case SCR_SONG:  scr_song_frame(); break;
    default:        scr_file_frame(); break;
    }
}

static uint8_t show_map;

static void global_keys(void)
{
    if (key_held & J_SELECT) {
        /* the screens form one row:  SYNTH < TRACK < LIVE > GEN > SONG > SAVE */
        if ((key_pressed & J_RIGHT) && app_screen + 1 < SCR_COUNT) { app_screen++; need_redraw = 1; show_map = 1; }
        if ((key_pressed & J_LEFT) && app_screen > 0) { app_screen--; need_redraw = 1; show_map = 1; }
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
        else { seq_seed_live(ui_entropy); seq_start(); }
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
    evo_rng = ui_entropy;            /* the moment START was pressed */
    seq_seed_live(ui_entropy);
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
            if (show_map) { show_map = 0; ui_msg_map(app_screen); }
        } else {
            frame_screen();
        }
        ui_header_update();
        ui_msg_tick();
        if (audition_timer && !--audition_timer) synth_gate_off();
        if (evo_commits != last_commits) {
            uint8_t g = proj.pat[seq_cur].gen;
            last_commits = evo_commits;
            sound_drift();
            strcpy(msgbuf, "EVOLVED  GEN ");
            msgbuf[13] = (char)('0' + (g / 100) % 10);
            msgbuf[14] = (char)('0' + (g / 10) % 10);
            msgbuf[15] = (char)('0' + g % 10);
            msgbuf[16] = 0;
            ui_msg(msgbuf);
        }
        if (proj.gen.mode == EVO_STABLE && (evo_cut_ofs || evo_tune_ofs[0] || evo_tune_ofs[1] ||
                                            evo_tune_ofs[2] || evo_tune_ofs[3]))
            sound_drift_reset();
        if (!evo_service()) regen_service();   /* one heavy job per pass at most */
        t0 = (uint8_t)((uint8_t)sys_time - t0);
        if (t0 > main_worst) main_worst = t0;
    }
}

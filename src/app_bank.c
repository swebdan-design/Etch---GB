/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Application services that need not live in bank 0 (ETCH 3.0 moved them
 * here to make room for the Game Boy Color palette code).
 */
#pragma bank 255
#include <gbdk/platform.h>
#include <string.h>
#include "engine.h"
#include "ui.h"
#include "corrosion.h"
#include "presets.h"

void anchors_forget(uint8_t pat);    /* main.c */


/* FILE > NEW: an empty project with a sensible starting sound */
void app_new_project(void) BANKED
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

void app_project_loaded(void) BANKED
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

void app_load_track(uint8_t t) BANKED
{
    uint8_t was = seq_playing;
    seq_stop();
    preset_load(t);
    app_project_loaded();
    if (was) seq_start();
}


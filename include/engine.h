/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Audio engine + sequencer interface (all of this lives in ROM bank 0
 * because it runs inside the timer interrupt).
 */
#ifndef ENGINE_H
#define ENGINE_H

#include "etch.h"

/* Engine tick: hardware timer, 4096 Hz input clock, TMA = 240 -> 256 Hz. */
#define TICK_HZ   256
#define TIMER_TMA 240

/* ---- the one project in working RAM ------------------------------------ */
extern project_t proj;

/* ---- bass voice (src/audio/engine.c) ----------------------------------- */
void synth_init(void);
void synth_note_on(uint8_t note, uint8_t accent);
void synth_glide(uint8_t note, uint8_t accent);
void synth_gate_off(void);
void synth_kill(void);
void synth_tick(void);
void synth_step_tick(void);      /* once per sequencer step (accent charge) */
uint8_t synth_wave_peek(uint8_t wave, uint8_t res, uint8_t cut, uint8_t *out16);
extern volatile uint8_t synth_gate;      /* 0 off, 1 held, 2 releasing */
extern volatile uint8_t synth_dbg_cut;   /* last cutoff index sent (UI meter) */
extern volatile uint8_t synth_dbg_res;
extern volatile uint8_t synth_dbg_amp;
extern volatile uint8_t synth_reloads;   /* wave-RAM reload counter         */

/* ---- drums (src/drums/drums.c) ----------------------------------------- */
#define KITS_PER_LANE 4
void drums_init(void);
void drums_play(uint8_t lanes, const uint8_t *kit);   /* lanes bitmask */
void drums_tick(void);
void drums_silence(void);
extern const char * const drum_kit_names[NUM_LANES][KITS_PER_LANE];
extern const uint8_t drum_kit_count[NUM_LANES];

/* ---- sequencer (src/sequencer/seq.c) ----------------------------------- */
extern volatile uint8_t seq_playing;
extern volatile uint8_t seq_step;          /* 0..15 currently sounding     */
extern volatile uint8_t seq_cur;           /* current pattern index        */
extern volatile uint8_t seq_queued;        /* 0xFF = none                  */
extern volatile uint8_t seq_chain_pos;
extern volatile uint8_t seq_chain_bar;
extern volatile uint8_t seq_loop;          /* times current pattern looped */
extern volatile uint8_t seq_bar_total;     /* free-running bar counter     */
extern volatile uint8_t seq_fill;          /* fill active                  */
extern volatile uint8_t seq_activity;      /* bit0 bass, bit1..4 lanes     */
extern volatile uint8_t seq_step_event;    /* incremented each step        */
extern volatile uint8_t seq_isr_max;       /* worst ISR length, DIV ticks  */
extern uint16_t         seq_fill_mask[NUM_LANES];

/* CORROSION hand-off between main loop and ISR */
#define EVO_IDLE   0     /* main loop must (re)compute a candidate          */
#define EVO_READY  1     /* evo_cand holds a mutation for evo_target        */
extern volatile uint8_t evo_state;
extern volatile uint8_t evo_target;
extern volatile uint8_t evo_request;       /* manual "evolve now"          */
extern volatile uint8_t evo_commits;       /* incremented on every commit  */
extern volatile uint8_t evo_undo_pat;      /* 0xFF = nothing to undo       */
extern pattern_t        evo_cand;
extern pattern_t        evo_undo;

void seq_init(void);
void seq_start(void);
void seq_stop(void);
void seq_set_tempo(void);                  /* after bpm/swing change       */
void seq_queue_pattern(uint8_t p);
void seq_jump_chain(uint8_t pos);
void seq_trigger_fill(const uint16_t *masks);
void seq_pattern_edited(void);             /* invalidates evo candidate    */
uint8_t seq_undo(void);
void seq_backup_for_undo(uint8_t pat);
pattern_t *seq_pattern(void);

#endif

/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Sequencer clock and step logic.  Runs entirely in the timer interrupt so
 * that UI drawing and input handling can never delay a step.
 *
 * Timing: the hardware timer fires at 256 Hz (4096 Hz / 16).  Every tick a
 * 16.8 fixed-point phase accumulator spanning a pair of 16th notes advances
 * by an increment derived from BPM.  The even 16th begins when it wraps, the
 * odd 16th when it passes the swing point.  Step onsets are quantised to one
 * tick (3.9 ms) but never accumulate error: the long-run tempo is exact to
 * within 0.002 %, and swing cannot change the tempo.
 *
 * Step semantics (TB-style):
 *   NOTE   new note; envelopes retrigger unless the previous step slid into it
 *   TIE    previous note sustains, no retrigger
 *   REST   silence; the previous note is released at its gate point
 *   SLIDE  on step n: gate held past the gate point and pitch glides into
 *          step n+1 if that step plays a note
 *   ACCENT louder + stronger, faster filter sweep (accent may combine with slide)
 *   PROB   note plays with probability (p+1)/8, rolled live each loop
 *   COND   1:2 2:2 1:4 ... play only on that loop of the pattern,
 *          FILL / !FILL play only while a fill is (not) active
 */
#include <gbdk/platform.h>
#include "engine.h"

project_t proj;

volatile uint8_t seq_playing;
volatile uint8_t seq_step;
volatile uint8_t seq_cur;
volatile uint8_t seq_queued = 0xFF;
volatile uint8_t seq_chain_pos;
volatile uint8_t seq_chain_bar;
volatile uint8_t seq_loop;
volatile uint8_t seq_bar_total;
volatile uint8_t seq_fill;
volatile uint8_t seq_fill_hold;      /* latched fill: runs until toggled off */
volatile uint8_t seq_activity;
volatile uint8_t seq_step_event;
volatile uint8_t seq_isr_max;
uint16_t seq_fill_mask[NUM_LANES];
static uint8_t fill_bars;
static uint8_t chain_jump = 0xFF;

volatile uint8_t evo_state;
volatile uint8_t evo_target;
volatile uint8_t evo_request;
volatile uint8_t evo_commits;
volatile uint8_t evo_undo_pat = 0xFF;
pattern_t evo_cand;
pattern_t evo_undo;
static uint8_t phrase_ctr;

static uint16_t pphase, pinc, swing_pt, gate_even, gate_odd;
static uint8_t  pfrac, pinc_frac;
static uint8_t gate_done;       /* gate point of this step handled          */
static uint8_t next_plays;      /* pre-rolled decision for the next step    */
static uint8_t cur_plays;       /* the current step's note actually sounds  */
static uint8_t legato;          /* gate held into the next step (slide)     */
static uint8_t pending_start;
static uint16_t live_rng = 0x5EED;

static const uint8_t step_bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
static const uint8_t gate_frac[4] = { 6, 9, 12, 15 };   /* gate length in 16ths of a step */

static uint8_t live_rand(void)
{
    live_rng ^= live_rng << 7;
    live_rng ^= live_rng >> 9;
    live_rng ^= live_rng << 8;
    return (uint8_t)live_rng;
}

/* the pattern that will play after the next bar line (chain / queue aware) */
static uint8_t peek_next_pattern(uint8_t *loop_out);
uint8_t seq_next_bar_pattern(void)
{
    uint8_t lp, p;
    CRITICAL { p = peek_next_pattern(&lp); }
    return p;
}

pattern_t *seq_pattern(void)
{
    return &proj.pat[seq_cur];
}

/* Next chain position holding a pattern (wraps; 0 if chain is empty). */
static uint8_t chain_next(uint8_t pos)
{
    uint8_t i;
    for (i = 0; i < CHAIN_LEN; i++) {
        pos = (pos + 1) & (CHAIN_LEN - 1);
        if (proj.chain.pat[pos] < NUM_PATTERNS) return pos;
        if (proj.chain.pat[pos] == 0xFF) pos = CHAIN_LEN - 1;  /* END -> wrap */
    }
    return 0;
}

/* Which pattern (and loop count) will play in the next bar. */
static uint8_t peek_next_pattern(uint8_t *loop_out)
{
    uint8_t p = seq_cur;
    *loop_out = seq_loop + 1;
    if (proj.chain.song && chain_jump != 0xFF) {
        if (proj.chain.pat[chain_jump] < NUM_PATTERNS) p = proj.chain.pat[chain_jump];
    } else if (proj.chain.song) {
        if ((uint8_t)(seq_chain_bar + 1) >= proj.chain.rep[seq_chain_pos]) {
            uint8_t np = proj.chain.pat[chain_next(seq_chain_pos)];
            if (np < NUM_PATTERNS) p = np;
        }
    } else if (seq_queued != 0xFF) {
        p = seq_queued;
    }
    if (p != seq_cur) *loop_out = 0;
    return p;
}

static uint8_t cond_ok(uint8_t cond, uint8_t loop)
{
    switch (cond) {
    case COND_1OF2: return (loop & 1) == 0;
    case COND_2OF2: return (loop & 1) == 1;
    case COND_1OF4: return (loop & 3) == 0;
    case COND_2OF4: return (loop & 3) == 1;
    case COND_3OF4: return (loop & 3) == 2;
    case COND_4OF4: return (loop & 3) == 3;
    case COND_FILL: return seq_fill;
    case COND_NOFILL: return !seq_fill;
    default: return 1;
    }
}

/* Decide whether step s of pattern p plays a new note (live dice). */
static uint8_t decide(const pattern_t *p, uint8_t s, uint8_t loop)
{
    uint8_t f = p->flags[s];
    uint8_t pr = p->prob[s];
    if ((f & STEP_TYPE_MASK) != STEP_NOTE) return 0;
    if (proj.mutes & 1) return 0;
    if (!cond_ok(pr >> 4, loop)) return 0;
    if ((pr & 7) != 7 && (live_rand() & 7) > (pr & 7)) return 0;
    return 1;
}

/* type of the next step (for tie look-ahead) */
static uint8_t next_type(void)
{
    uint8_t lp;
    const pattern_t *np;
    if (seq_step == 15) np = &proj.pat[peek_next_pattern(&lp)];
    else np = &proj.pat[seq_cur];
    return np->flags[(seq_step + 1) & 15] & STEP_TYPE_MASK;
}

static void bar_boundary(void)
{
    uint8_t changed = 0;
    uint8_t prev = seq_cur;

    seq_bar_total++;
    if (fill_bars && !seq_fill_hold) {
        if (--fill_bars == 0) seq_fill = 0;
    }
    if (proj.chain.song && chain_jump != 0xFF) {
        seq_chain_pos = chain_jump;
        seq_chain_bar = 0;
        chain_jump = 0xFF;
        if (proj.chain.pat[seq_chain_pos] < NUM_PATTERNS) seq_cur = proj.chain.pat[seq_chain_pos];
        seq_queued = 0xFF;
    } else if (proj.chain.song) {
        if (++seq_chain_bar >= proj.chain.rep[seq_chain_pos]) {
            uint8_t np;
            seq_chain_bar = 0;
            seq_chain_pos = chain_next(seq_chain_pos);
            np = proj.chain.pat[seq_chain_pos];
            if (np < NUM_PATTERNS) seq_cur = np;
        }
        seq_queued = 0xFF;
    } else if (seq_queued != 0xFF) {
        seq_cur = seq_queued;
        seq_queued = 0xFF;
    }
    if (seq_cur != prev) changed = 1;
    if (changed) seq_loop = 0; else seq_loop++;

    phrase_ctr++;
}

/* CORROSION commit.  Runs at the gate point of step 16 (a tick with no step
 * work) so the two 64-byte copies never share a tick with a step; the new
 * material therefore takes effect exactly on the next downbeat. */
static void evo_commit_point(void)
{
    uint8_t due, lp, np, nphrase = (uint8_t)(phrase_ctr + 1);
    due = evo_request;
    if (proj.gen.mode == EVO_DRIFT && (nphrase & 3) == 0) due = 1;
    if (proj.gen.mode == EVO_CORRODE && (nphrase & 1) == 0) due = 1;
    if (!due || evo_state != EVO_READY) return;
    np = peek_next_pattern(&lp);
    if (evo_target != np || (proj.pat[np].lock & PLOCK_ALL)) return;
    memcpy(&evo_undo, &proj.pat[np], sizeof(pattern_t));
    memcpy(&proj.pat[np], &evo_cand, sizeof(pattern_t));
    evo_undo_pat = np;
    evo_state = EVO_IDLE;
    evo_request = 0;
    evo_commits++;
    next_plays = decide(&proj.pat[np], 0, lp);   /* re-roll the new downbeat */
}

static void step_advance(void)
{
    const pattern_t *p;
    uint8_t s, f, lanes, lp;

    if (pending_start) {
        pending_start = 0;
        seq_step = 0;
        cur_plays = 0;
        legato = 0;
        next_plays = decide(&proj.pat[seq_cur], 0, seq_loop);
    } else {
        seq_step = (seq_step + 1) & 15;
        if (seq_step == 0) bar_boundary();
    }
    s = seq_step;
    p = &proj.pat[seq_cur];
    gate_done = 0;
    seq_activity = 0;
    seq_step_event++;
    synth_step_tick();

    /* ---- drums ----------------------------------------------------- */
    {
        const uint16_t *m = seq_fill ? seq_fill_mask : p->drum;
        uint8_t mut = proj.mutes;
        lanes = 0;
        if (s < 8) {
            uint8_t b = step_bit[s];
            if (((const uint8_t *)&m[0])[0] & b) lanes |= 1;
            if (((const uint8_t *)&m[1])[0] & b) lanes |= 2;
            if (((const uint8_t *)&m[2])[0] & b) lanes |= 4;
            if (((const uint8_t *)&m[3])[0] & b) lanes |= 8;
        } else {
            uint8_t b = step_bit[s - 8];
            if (((const uint8_t *)&m[0])[1] & b) lanes |= 1;
            if (((const uint8_t *)&m[1])[1] & b) lanes |= 2;
            if (((const uint8_t *)&m[2])[1] & b) lanes |= 4;
            if (((const uint8_t *)&m[3])[1] & b) lanes |= 8;
        }
        lanes &= ~(mut >> 1);
    }
    if (lanes) drums_play(lanes, p->kit);
    seq_activity = lanes << 1;

    /* ---- bass ------------------------------------------------------ */
    f = p->flags[s];
    if (next_plays) {
        if (legato) synth_glide(p->note[s], f & STEP_ACCENT);
        else synth_note_on(p->note[s], f & STEP_ACCENT);
        cur_plays = 1;
        seq_activity |= 1;
    } else if ((f & STEP_TYPE_MASK) == STEP_TIE && cur_plays && synth_gate == 1) {
        /* sustain: nothing to do, cur_plays stays */
    } else {
        cur_plays = 0;
        synth_gate_off();
    }
    legato = 0;

    /* pre-roll next step's dice */
    if (s == 15) {
        uint8_t np = peek_next_pattern(&lp);
        next_plays = decide(&proj.pat[np], 0, lp);
    } else {
        next_plays = decide(p, s + 1, seq_loop);
    }
}

static void gate_point(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    uint8_t f;
    gate_done = 1;
    if (seq_step == 15) evo_commit_point();
    f = p->flags[seq_step];
    if (!cur_plays) return;
    if ((f & STEP_SLIDE) && next_plays) {
        legato = 1;                       /* hold + glide into next note */
        return;
    }
    if (next_type() == STEP_TIE) return;  /* sustain across tie          */
    synth_gate_off();
}

volatile uint16_t isr_count;
volatile uint8_t isr_late;          /* entries > 1.5 periods after the previous one */
static uint8_t isr_prev;
volatile uint16_t isr_div_sum;
static void timer_isr(void)
{
    uint8_t t0 = DIV_REG;
    uint16_t old;
    if ((uint8_t)(t0 - isr_prev) > 96 && isr_count) isr_late++;
    isr_prev = t0;
    isr_count++;
    if (seq_playing) {
        if (pending_start) {
            step_advance();
        } else {
            /* 16.8 fixed-point phase over a PAIR of 16ths */
            uint8_t f = pfrac;
            old = pphase;
            pfrac += pinc_frac;
            pphase += pinc + (pfrac < f ? 1 : 0);
            if (pphase < old) {
                step_advance();                     /* even step: pair start   */
            } else if (old < swing_pt && pphase >= swing_pt) {
                step_advance();                     /* odd step: swing point   */
            } else if (!gate_done && pphase >= ((seq_step & 1) ? gate_odd : gate_even)) {
                gate_point();
            }
        }
    }
    synth_tick();
    drums_tick();
    t0 = DIV_REG - t0;
    if (t0 > seq_isr_max) seq_isr_max = t0;
    isr_div_sum += t0;
}

/* Tempo, swing and gate length.
 * One accumulator turn (65536) = two straight 16ths, so its increment per
 * 256 Hz tick is bpm * 2/15 * 65536 / 256 / 2 = bpm * 8.5333 (16.8 fixed).
 * The even 16th starts at 0, the odd one at swing_pt: swing moves only the
 * offbeat and can never change the tempo or drift against the bar. */
void seq_set_tempo(void)
{
    uint8_t bpm = proj.synth.bpm;
    uint8_t sw = proj.synth.swing;
    uint8_t g = gate_frac[proj.synth.gate & 3];
    uint32_t inc24;
    uint16_t sp, ge, go;
    if (bpm < 60) bpm = 60;
    if (bpm > 220) bpm = 220;
    if (sw > 8) sw = 8;
    inc24 = ((uint32_t)bpm * 4369UL) >> 1;          /* bpm * 8.5333 * 256 */
    sp = (uint16_t)(0x8000u + (uint16_t)sw * 1311u); /* +4% of a pair per unit */
    ge = (uint16_t)(((uint32_t)sp * g) >> 4);
    go = (uint16_t)(sp + ((((uint32_t)0x10000UL - sp) * g) >> 4));
    CRITICAL {
        pinc = (uint16_t)(inc24 >> 8);
        pinc_frac = (uint8_t)inc24;
        swing_pt = sp;
        gate_even = ge;
        gate_odd = go;
    }
}

void seq_init(void)
{
    seq_playing = 0;
    seq_isr_max = 0;
    seq_set_tempo();
    CRITICAL {
        add_TIM(timer_isr);
    }
    TMA_REG = TIMER_TMA;
    TAC_REG = 0x04;                     /* timer on, 4096 Hz */
    set_interrupts(VBL_IFLAG | TIM_IFLAG);
}

void seq_seed_live(uint16_t s)
{
    live_rng = s ? s : 0x5EED;
}

void seq_start(void)
{
    CRITICAL {
        seq_loop = 0;
        phrase_ctr = 0;
        pphase = 0;
        pfrac = 0;
        seq_chain_bar = 0;
        if (proj.chain.song) {
            if (proj.chain.pat[seq_chain_pos] >= NUM_PATTERNS) seq_chain_pos = 0;
            if (proj.chain.pat[seq_chain_pos] < NUM_PATTERNS)
                seq_cur = proj.chain.pat[seq_chain_pos];
        }
        pending_start = 1;
        seq_playing = 1;
    }
}

void seq_stop(void)
{
    CRITICAL {
        seq_playing = 0;
        seq_fill = 0;
        seq_fill_hold = 0;
        fill_bars = 0;
        synth_gate_off();
        drums_silence();
    }
}

void seq_queue_pattern(uint8_t p)
{
    if (p >= NUM_PATTERNS) return;
    CRITICAL {
        if (!seq_playing) { seq_cur = p; seq_queued = 0xFF; }
        else seq_queued = p;
    }
}

void seq_jump_chain(uint8_t pos)
{
    pos &= CHAIN_LEN - 1;
    if (proj.chain.pat[pos] >= NUM_PATTERNS) return;
    CRITICAL {
        if (!seq_playing) {
            seq_chain_pos = pos;
            seq_chain_bar = 0;
            seq_cur = proj.chain.pat[pos];
        } else {
            chain_jump = pos;             /* honoured at the next bar line */
        }
    }
}

/* Latched fill on/off.  On: the fill plays bar after bar until switched off
 * (the main loop supplies a fresh variation each bar via seq_fill_masks).
 * Off: the groove returns immediately. */
void seq_fill_latch(uint8_t on, const uint16_t *masks)
{
    CRITICAL {
        if (on) {
            memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask));
            seq_fill_hold = 1;
            seq_fill = 1;
            fill_bars = 1;
        } else {
            seq_fill_hold = 0;
            seq_fill = 0;
            fill_bars = 0;
        }
    }
}

void seq_fill_masks(const uint16_t *masks)
{
    CRITICAL { memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask)); }
}

void seq_trigger_fill(const uint16_t *masks)
{
    CRITICAL {
        memcpy(seq_fill_mask, masks, sizeof(seq_fill_mask));
        /* a fill asked for late in the bar runs through the next bar too */
        fill_bars = (seq_playing && seq_step >= 12) ? 2 : 1;
        seq_fill = 1;
    }
}

void seq_pattern_edited(void)
{
    evo_state = EVO_IDLE;
}

/* Keep the current contents of pattern pat as the undo state (used before
 * GENERATE or other destructive whole-pattern operations). */
void seq_backup_for_undo(uint8_t pat)
{
    CRITICAL {
        memcpy(&evo_undo, &proj.pat[pat], sizeof(pattern_t));
        evo_undo_pat = pat;
        evo_state = EVO_IDLE;
    }
}

/* Restore the previous generation.  Returns 1 if something was undone. */
uint8_t seq_undo(void)
{
    uint8_t r = 0;
    CRITICAL {
        if (evo_undo_pat < NUM_PATTERNS) {
            memcpy(&proj.pat[evo_undo_pat], &evo_undo, sizeof(pattern_t));
            evo_undo_pat = 0xFF;
            evo_state = EVO_IDLE;
            evo_request = 0;
            r = 1;
        }
    }
    return r;
}

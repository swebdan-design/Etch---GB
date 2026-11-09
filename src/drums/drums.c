/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Drum synthesis on CH1 (kick), CH2 (snare/rim/perc body) and CH4 (noise).
 *
 * Four logical lanes share three physical channels:
 *   KICK  lane -> CH1 square with a software pitch envelope (256 Hz)
 *   SNARE lane -> CH2 tone body and/or CH4 noise burst (clap = retriggered noise)
 *   CH    lane -> CH4 noise (length counter gives short closed hats)
 *   OH    lane -> CH4 noise, or CH2 for the tonal BLIP percussion
 * When several lanes need the same channel on one step, the highest priority
 * wins: CH4: SNARE > OH > CH, CH2: SNARE > OH.  A newer hit on a channel
 * always replaces a ringing older one (closed hat chokes open hat).
 */
#include <gbdk/platform.h>
#include "engine.h"

#define USE_TONE1 0x01
#define USE_TONE2 0x02
#define USE_NOISE 0x04

typedef struct {
    uint8_t  use;
    uint8_t  duty;        /* NR11 / NR21                                */
    uint8_t  tenv;        /* NR12 / NR22                                */
    uint16_t f0;          /* tone period start (11 bit)                 */
    uint16_t f1;          /* tone period end                            */
    uint8_t  fshift;      /* pitch env speed: d -= d >> fshift          */
    uint8_t  nlen;        /* NR41 (0 = no length cut)                   */
    uint8_t  nenv;        /* NR42                                       */
    uint8_t  npoly;       /* NR43                                       */
    uint8_t  retrig;      /* claps: number of extra noise retriggers    */
    uint8_t  pad[3];      /* keep sizeof == 16 (index by shift)         */
} drum_def_t;

/* lane-major table: [lane][kit] */
static const drum_def_t kits[NUM_LANES][KITS_PER_LANE] = {
    {   /* KICK */
        { USE_TONE1, 0x80, 0xF3, 1560, 0,    3, 0, 0, 0, 0 },  /* DEEP  */
        { USE_TONE1, 0x80, 0xF1, 1720, 240,  2, 0, 0, 0, 0 },  /* PUNCH */
        { USE_TONE1, 0x40, 0xF2, 1300, 60,   2, 0, 0, 0, 0 },  /* THUD  */
        { USE_TONE1 | USE_NOISE, 0x80, 0xF1, 1800, 300, 1, 0x3A, 0x61, 0x00, 0 }, /* CLICK */
    },
    {   /* SNARE */
        { USE_TONE2 | USE_NOISE, 0x80, 0xA1, 1640, 1560, 2, 0, 0xB1, 0x21, 0 },  /* SHORT */
        { USE_TONE2 | USE_NOISE, 0x40, 0x92, 1560, 1450, 2, 0, 0xD2, 0x52, 0 },  /* GRIT  */
        { USE_NOISE,             0x00, 0x00, 0,    0,    0, 0, 0xA2, 0x31, 2 },  /* CLAP  */
        { USE_TONE2,             0x00, 0x81, 1950, 1940, 3, 0, 0,    0,    0 },  /* RIM   */
    },
    {   /* CLOSED HAT */
        { USE_NOISE, 0, 0, 0, 0, 0, 0x36, 0xA1, 0x00, 0 },     /* CLOSED */
        { USE_NOISE, 0, 0, 0, 0, 0, 0x3B, 0x91, 0x0A, 0 },     /* TICK (7-bit metallic) */
        { USE_NOISE, 0, 0, 0, 0, 0, 0x2C, 0x52, 0x11, 0 },     /* SHAKE */
        { USE_NOISE, 0, 0, 0, 0, 0, 0x3D, 0xC1, 0x01, 0 },     /* SHARP */
    },
    {   /* OPEN HAT / PERC */
        { USE_NOISE, 0, 0, 0, 0, 0, 0, 0x93, 0x01, 0 },        /* OPEN  */
        { USE_NOISE, 0, 0, 0, 0, 0, 0, 0x84, 0x1B, 0 },        /* METAL */
        { USE_TONE2, 0x80, 0x72, 1899, 1890, 3, 0, 0, 0, 0 },  /* BLIP  */
        { USE_NOISE, 0, 0, 0, 0, 0, 0, 0xA6, 0x23, 0 },        /* CRASH */
    },
};

const char * const drum_kit_names[NUM_LANES][KITS_PER_LANE] = {
    { "DEEP ", "PUNCH", "THUD ", "CLICK" },
    { "SHORT", "GRIT ", "CLAP ", "RIM  " },
    { "CLOSD", "TICK ", "SHAKE", "SHARP" },
    { "OPEN ", "METAL", "BLIP ", "CRASH" },
};
const uint8_t drum_kit_count[NUM_LANES] = { 4, 4, 4, 4 };

/* per-channel tone pitch-envelope state */
static uint16_t t1_per, t1_end; static uint8_t t1_shift, t1_on;
static uint16_t t2_per, t2_end; static uint8_t t2_shift, t2_on;
static uint8_t  clap_left, clap_timer, clap_env, clap_poly;

void drums_init(void)
{
    t1_on = t2_on = 0;
    clap_left = 0;
    NR12_REG = 0; NR22_REG = 0; NR42_REG = 0;
    NR10_REG = 0x00;                  /* no hardware sweep */
}

void drums_silence(void)
{
    drums_init();
}

static void tone1(const drum_def_t *d)
{
    t1_per = d->f0; t1_end = d->f1; t1_shift = d->fshift; t1_on = 1;
    NR10_REG = 0x00;
    NR11_REG = d->duty;
    NR12_REG = d->tenv;
    NR13_REG = (uint8_t)t1_per;
    NR14_REG = 0x80 | (uint8_t)(t1_per >> 8);
}

static void tone2(const drum_def_t *d)
{
    t2_per = d->f0; t2_end = d->f1; t2_shift = d->fshift; t2_on = 1;
    NR21_REG = d->duty;
    NR22_REG = d->tenv;
    NR23_REG = (uint8_t)t2_per;
    NR24_REG = 0x80 | (uint8_t)(t2_per >> 8);
}

static void noise(const drum_def_t *d)
{
    NR41_REG = d->nlen;
    NR42_REG = d->nenv;
    NR43_REG = d->npoly;
    NR44_REG = d->nlen ? 0xC0 : 0x80;
    clap_left = d->retrig;
    if (clap_left) {
        clap_timer = 0;
        clap_env = d->nenv;
        clap_poly = d->npoly;
    }
}

void drums_play(uint8_t lanes, const uint8_t *kit)
{
    const drum_def_t *noise_src = 0;
    const drum_def_t *tone2_src = 0;
    const drum_def_t *d;

    /* lowest priority first, higher priority overwrites */
    if (lanes & (1 << LANE_CH)) {
        d = &kits[LANE_CH][kit[LANE_CH] & 3];
        noise_src = d;
    }
    if (lanes & (1 << LANE_OH)) {
        d = &kits[LANE_OH][kit[LANE_OH] & 3];
        if (d->use & USE_NOISE) noise_src = d;
        if (d->use & USE_TONE2) tone2_src = d;
    }
    if (lanes & (1 << LANE_SNARE)) {
        d = &kits[LANE_SNARE][kit[LANE_SNARE] & 3];
        if (d->use & USE_NOISE) noise_src = d;
        if (d->use & USE_TONE2) tone2_src = d;
    }
    if (lanes & (1 << LANE_KICK)) {
        d = &kits[LANE_KICK][kit[LANE_KICK] & 3];
        tone1(d);
        /* CLICK kick adds a tiny noise transient only if CH4 is free */
        if ((d->use & USE_NOISE) && !noise_src) noise_src = d;
    }
    if (tone2_src) tone2(tone2_src);
    if (noise_src) noise(noise_src);
}

void drums_tick(void)
{
    uint16_t dlt;
    if (t1_on) {
        if (t1_per > t1_end) {
            dlt = t1_per - t1_end;
            dlt -= (dlt >> t1_shift) + 1;
            t1_per = t1_end + dlt;
        } else {
            t1_on = 0;
        }
        NR13_REG = (uint8_t)t1_per;
        NR14_REG = (uint8_t)(t1_per >> 8) & 7;
    }
    if (t2_on) {
        if (t2_per > t2_end) {
            dlt = t2_per - t2_end;
            dlt -= (dlt >> t2_shift) + 1;
            t2_per = t2_end + dlt;
        } else {
            t2_on = 0;
        }
        NR23_REG = (uint8_t)t2_per;
        NR24_REG = (uint8_t)(t2_per >> 8) & 7;
    }
    if (clap_left) {
        if (++clap_timer >= 3) {          /* ~12 ms between clap slaps */
            clap_timer = 0;
            clap_left--;
            NR42_REG = clap_left ? clap_env : (uint8_t)((clap_env & 0xF0) | 0x03);
            NR43_REG = clap_poly;
            NR44_REG = 0x80;
        }
    }
}

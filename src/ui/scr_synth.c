/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * SYNTH: one page per channel.
 *
 *   row 2         channel tabs  BAS KCK SNR HAT OPN
 *   BASS page     the acid voice (+ live scope of the playing wavetable)
 *   drum pages    SOUND (this pattern's kit), TUNE, DECAY, LEVEL, MUTE, LOCK
 *
 *   UP/DOWN       select          </> or A+</>   adjust by 1
 *   A+UP/DOWN     coarse          B+A            back to default
 *   B+</>         previous / next channel        A (tap) audition
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"

#define N_CH 5
#define N_BASS 14
#define N_DRUM 6
static const char * const tab_n[N_CH] = { "BAS", "KCK", "SNR", "HAT", "OPN" };
static const char * const ch_hw[N_CH] = {
    "CH3 WAVETABLE ACID ", "CH1 PULSE + PITCH  ", "CH2 TONE + CH4 NOISE", "CH4 NOISE          ", "CH4 NOISE / CH2    "
};

/* ---- bass page ----------------------------------------------------------- */
static const char * const blbl[N_BASS] = {
    "WAVE  ", "CUTOFF", "RESO  ", "ENVMOD", "DECAY ", "ACCENT", "DRIVE ", "SLIDE ",
    "GATE  ", "LFO RT", "LFO DP", "LFO TO", "LFO SH", "TUNE  "
};
/* screen order -> byte offset inside synth_t, range, default */
static const uint8_t boff[N_BASS] = { 0, 1, 2, 3, 4, 5, 6, 7, 15, 8, 9, 10, 11, 14 };
static const uint8_t bmax[N_BASS] = { 3, 31, 7, 15, 15, 15, 2, 15, 3, 15, 15, 3, 3, 24 };
static const uint8_t bdef[N_BASS] = { 0, 8, 4, 8, 7, 8, 0, 6, 1, 4, 0, 0, 0, 12 };
static const char * const wave_n[4]  = { "SAW ", "SQR ", "HOLW", "BITE" };
static const char * const drive_n[3] = { "OFF ", "SOFT", "HARD" };
static const char * const gate_n[4]  = { "SHRT", "MED ", "LONG", "FULL" };
static const char * const dest_n[4]  = { "OFF ", "PTCH", "CUT ", "RES " };
static const char * const shape_n[4] = { "TRI ", "SQR ", "SAW ", "S&H " };

/* ---- drum pages -------------------------------------------------------- */
static const char * const dlbl[N_DRUM] = { "SOUND ", "TUNE  ", "DECAY ", "LEVEL ", "MUTE  ", "LOCK  " };

static uint8_t ch, cur, scope_div, sc_w = 0xFF, sc_c, sc_r;
static uint8_t scope_buf[128];

static uint8_t *bptr(uint8_t i) { return (uint8_t *)&proj.synth + boff[i]; }

static uint8_t nrows(void) { return ch ? N_DRUM : N_BASS; }

static void draw_tabs(void)
{
    uint8_t i;
    ui_fill(0, 2, SCR_W, 0);
    for (i = 0; i < N_CH; i++) {
        ui_text((uint8_t)(i * 4), 2, tab_n[i]);
        if (i == ch) ui_invert((uint8_t)(i * 4), 2, 3);
    }
    if (cur == 0xFF) ui_put(19, 2, T_ARROW_L);
}

static void draw_bass(uint8_t i)
{
    uint8_t y = (uint8_t)(3 + i), v = *bptr(i);
    const char *name = 0;
    ui_text(0, y, blbl[i]);
    if (cur == i) ui_invert(0, y, 6);
    ui_put(6, y, 0);
    switch (i) {
    case 0: name = wave_n[v & 3]; break;
    case 6: name = drive_n[v > 2 ? 2 : v]; break;
    case 8: name = gate_n[v & 3]; break;
    case 11: name = dest_n[v & 3]; break;
    case 12: name = shape_n[v & 3]; break;
    }
    if (name) ui_textn(7, y, name, 4);
    else if (i == 13) {
        ui_put(7, y, v >= 12 ? ('+' - 32) : ('-' - 32));
        ui_num(8, y, v >= 12 ? (uint16_t)(v - 12) : (uint16_t)(12 - v), 2);
        ui_put(10, y, 0);
    } else {
        ui_num(7, y, v, 3);
        ui_put(10, y, 0);
    }
    ui_meter(11, y, 3, v, bmax[i]);
}

static uint8_t dval(uint8_t i)
{
    uint8_t l = (uint8_t)(ch - 1);
    switch (i) {
    case 0: return proj.pat[seq_cur].kit[l] & 3;
    case 1: return proj.drum_tune[l];
    case 2: return proj.drum_decay[l];
    case 3: return proj.drum_level[l];
    case 4: return (proj.mutes >> ch) & 1;
    default: return (proj.pat[seq_cur].lock >> ch) & 1;      /* PLOCK_LANE0 << l == 2 << l */
    }
}

static void draw_drum(uint8_t i)
{
    uint8_t y = (uint8_t)(4 + i * 2), v = dval(i), l = (uint8_t)(ch - 1);
    ui_fill(0, y, SCR_W, 0);
    ui_text(0, y, dlbl[i]);
    if (cur == i) ui_invert(0, y, 6);
    switch (i) {
    case 0:
        ui_textn(7, y, drum_kit_names[l][v], 5);
        ui_text(13, y, "(P");
        ui_num0(15, y, (uint16_t)(seq_cur + 1), 2);
        ui_put(17, y, ')' - 32);
        break;
    case 1:
        ui_put(7, y, v >= 8 ? ('+' - 32) : ('-' - 32));
        ui_num(8, y, v >= 8 ? (uint16_t)(v - 8) : (uint16_t)(8 - v), 1);
        ui_meter(11, y, 6, v, 15);
        break;
    case 2: case 3:
        ui_num(7, y, v, 2);
        ui_meter(11, y, 6, v, 15);
        break;
    default:
        ui_text(7, y, v ? "ON " : "OFF");
        break;
    }
}

static void scope_render(uint8_t wave, uint8_t res, uint8_t cut)
{
    uint8_t raw[16], smp[32], i, x, y, y0, y1, prev;
    synth_wave_peek(wave, res, cut, raw);
    for (i = 0; i < 16; i++) {
        smp[i * 2] = raw[i] >> 4;
        smp[i * 2 + 1] = raw[i] & 15;
    }
    memset(scope_buf, 0, sizeof(scope_buf));
    for (x = 1; x < 32; x += 2) scope_buf[(4 + (x >> 3)) * 16] |= (uint8_t)(0x80 >> (x & 7));
    prev = (uint8_t)(15 - smp[0]);
    for (x = 0; x < 32; x++) {
        y = (uint8_t)(15 - smp[x]);
        y0 = y < prev ? y : prev;
        y1 = y < prev ? prev : y;
        if (x == 0) { y0 = y1 = y; }
        for (; y0 <= y1; y0++) {
            uint8_t *t = &scope_buf[((y0 >> 3) * 4 + (x >> 3)) * 16 + ((y0 & 7) << 1)];
            uint8_t bit = (uint8_t)(0x80 >> (x & 7));
            t[0] |= bit;
            t[1] |= bit;
        }
        prev = y;
    }
    ui_vram_tiles(T_SCOPE_BASE, 8, scope_buf);
}

static void draw_page(void)
{
    uint8_t i;
    for (i = 3; i < 17; i++) ui_fill(0, i, SCR_W, 0);
    draw_tabs();
    if (ch == 0) {
        for (i = 0; i < N_BASS; i++) draw_bass(i);
        ui_text(15, 3, "SCOPE");
        for (i = 0; i < 4; i++) {
            ui_put((uint8_t)(15 + i), 4, (uint8_t)(T_SCOPE_BASE + i));
            ui_put((uint8_t)(15 + i), 5, (uint8_t)(T_SCOPE_BASE + 4 + i));
        }
        ui_put(14, 4, T_VBAR); ui_put(14, 5, T_VBAR);
        ui_put(19, 4, T_VBAR); ui_put(19, 5, T_VBAR);
        sc_w = 0xFF;
    } else {
        for (i = 0; i < N_DRUM; i++) draw_drum(i);
        ui_textn(0, 16, ch_hw[ch], 20);
    }
    ui_text(0, 17, ch ? "A:HEAR B+</>:CHANNEL" : "A:HEAR B+</>:CHANNEL");
}

void scr_synth_enter(void) BANKED
{
    ui_cls();
    ui_header("SYNTH");
    if (cur != 0xFF && cur >= nrows()) cur = 0;
    draw_page();
}

static void audition(void)
{
    if (ch == 0) app_audition(proj.pat[seq_cur].note[0], 1);
    else drums_play((uint8_t)(1 << (ch - 1)), proj.pat[seq_cur].kit);
}

static void bass_adjust(uint8_t i, int8_t d, uint8_t reset)
{
    uint8_t *p = bptr(i);
    if (reset) *p = bdef[i];
    else {
        if (bmax[i] <= 7) d = d > 0 ? 1 : -1;
        if (i == 13 && (d > 1 || d < -1)) d = d > 0 ? 12 : -12;
        *p = clampadd(*p, d, 0, bmax[i]);
    }
    if (i == 8) seq_set_tempo();            /* gate length lives in the clock */
    draw_bass(i);
}

static void drum_adjust(uint8_t i, int8_t d, uint8_t reset)
{
    uint8_t l = (uint8_t)(ch - 1);
    pattern_t *p = &proj.pat[seq_cur];
    switch (i) {
    case 0:
        p->kit[l] = reset ? 0 : (uint8_t)((p->kit[l] + (d > 0 ? 1 : 3)) & 3);
        app_pattern_edited();
        break;
    case 1: proj.drum_tune[l] = reset ? 8 : clampadd(proj.drum_tune[l], d, 0, 15); break;
    case 2: proj.drum_decay[l] = reset ? 8 : clampadd(proj.drum_decay[l], d, 0, 15); break;
    case 3: proj.drum_level[l] = reset ? 15 : clampadd(proj.drum_level[l], d, 0, 15); break;
    case 4:
        if (reset) proj.mutes &= (uint8_t)~(1 << ch);
        else proj.mutes ^= (uint8_t)(1 << ch);
        break;
    default:
        if (reset) p->lock &= (uint8_t)~(PLOCK_LANE0 << l);
        else p->lock ^= (uint8_t)(PLOCK_LANE0 << l);
        app_pattern_edited();
        break;
    }
    drums_revoice();
    draw_drum(i);
    if (i < 4) audition();
}

void scr_synth_frame(void) BANKED
{
    uint8_t old = cur, nav, pg, n = nrows(), cut, res;
    int8_t d = 0;

    pg = ui_page();
    if (pg & (J_LEFT | J_RIGHT)) {
        ch = (uint8_t)((ch + ((pg & J_RIGHT) ? 1 : N_CH - 1)) % N_CH);
        if (cur != 0xFF) cur = 0;
        draw_page();
        return;
    }
    nav = ui_nav();
    if (nav & J_UP) cur = (cur == 0xFF) ? (uint8_t)(n - 1) : (cur == 0 ? 0xFF : (uint8_t)(cur - 1));
    if (nav & J_DOWN) cur = (cur == 0xFF) ? 0 : (cur + 1 >= n ? 0xFF : (uint8_t)(cur + 1));
    if (cur == 0xFF) {
        /* on the tab row LEFT/RIGHT (or A+</>) changes channel */
        d = (nav & J_RIGHT) ? 1 : (nav & J_LEFT) ? -1 : ui_edit(1);
        if (d) { ch = (uint8_t)((ch + (d > 0 ? 1 : N_CH - 1)) % N_CH); draw_page(); return; }
        if (ui_a_tap()) audition();
    } else {
        if (nav & J_RIGHT) d = 1;
        if (nav & J_LEFT) d = -1;
        if (!d) d = ui_edit(4);
        if (ch == 0) {
            if (d) bass_adjust(cur, d, 0);
            if (ui_clear()) bass_adjust(cur, 0, 1);
            if (ui_a_tap()) audition();
        } else {
            if (d) drum_adjust(cur, d, 0);
            if (ui_clear()) drum_adjust(cur, 0, 1);
            if (cur >= 4) { if (ui_a_press()) { ui_a_consume(); drum_adjust(cur, 1, 0); } }
            else if (ui_a_tap()) audition();
        }
    }
    if (old != cur) {
        draw_tabs();
        if (ch == 0) { if (old < N_BASS) draw_bass(old); if (cur < N_BASS) draw_bass(cur); }
        else { if (old < N_DRUM) draw_drum(old); if (cur < N_DRUM) draw_drum(cur); }
    }

    /* live scope: the table the engine is actually playing */
    if (ch == 0 && ++scope_div >= 3) {
        scope_div = 0;
        if (synth_gate) { cut = synth_dbg_cut; res = synth_dbg_res; }
        else { cut = proj.synth.cutoff; res = proj.synth.res; }
        if (sc_w != (proj.synth.wave & 3) || sc_c != cut || sc_r != res) {
            sc_w = proj.synth.wave & 3; sc_c = cut; sc_r = res;
            scope_render(sc_w, res, cut);
            ui_put(15, 6, 'C' - 32);
            ui_num(16, 6, cut, 2);
            ui_put(18, 6, 'R' - 32);
            ui_put(19, 6, (uint8_t)('0' - 32 + res));
        }
    }
}

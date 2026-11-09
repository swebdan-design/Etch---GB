/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * GEN: the CORROSION generative system.
 *
 *   KEY .. MODE     LEFT/RIGHT or B+UP/DOWN adjust
 *   SEED            B+UP/DOWN +-1, B+LEFT/RIGHT +-256, A = new seed + generate
 *   CODE            LEFT/RIGHT pick a digit, B+UP/DOWN change it
 *   action rows     LEFT/RIGHT pick, A executes
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"
#include "engine.h"
#include "corrosion.h"

#define R_KEY 0
#define R_SCALE 1
#define R_STYLE 2
#define R_OCT 3
#define R_DENS 4
#define R_SYNC 5
#define R_ACC 6
#define R_SLIDE 7
#define R_MUT 8
#define R_MODE 9
#define R_SEED 10
#define R_CODE 11
#define R_ACT1 12
#define R_ACT2 13
#define R_ACT3 14
#define N_ROWS 15

static const char * const lbl[R_CODE + 1] = {
    "KEY    ", "SCALE  ", "STYLE  ", "OCTAVE ", "DENSITY", "SYNCOP ",
    "ACCENT ", "SLIDE  ", "MUTATE ", "MODE   ", "SEED   ", "CODE   "
};
static const char * const key_n[12] = { "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
static const char * const act_n[3][2] = {
    { "GEN ALL  ", "GEN BASS " },
    { "EVOLVE   ", "UNDO     " },
    { "LOCK BASS", "LOCK PATT" },
};

static uint8_t cur, col, last_pat = 0xFF, last_commits;

static uint8_t *gptr(uint8_t r)
{
    switch (r) {
    case R_KEY: return &proj.gen.key;
    case R_SCALE: return &proj.gen.scale;
    case R_STYLE: return &proj.gen.style;
    case R_OCT: return &proj.gen.octave;
    case R_DENS: return &proj.gen.density;
    case R_SYNC: return &proj.gen.sync;
    case R_ACC: return &proj.gen.accent;
    case R_SLIDE: return &proj.gen.slide;
    case R_MUT: return &proj.gen.mutation;
    default: return &proj.gen.mode;
    }
}
static const uint8_t gmax[R_MODE + 1] = { 11, SCALE_COUNT - 1, STYLE_COUNT - 1, 2, 15, 15, 15, 15, 15, EVO_COUNT - 1 };

static void draw_row(uint8_t r)
{
    uint8_t y = (uint8_t)(2 + r);
    const pattern_t *p = &proj.pat[seq_cur];
    if (r <= R_CODE) {
        ui_text(0, y, lbl[r]);
        if (cur == r) ui_invert(0, y, 7);
        ui_put(7, y, 0);
    }
    switch (r) {
    case R_KEY: ui_textn(8, y, key_n[proj.gen.key % 12], 12); break;
    case R_SCALE: ui_textn(8, y, corr_scale_names[proj.gen.scale], 12); break;
    case R_STYLE: ui_textn(8, y, corr_style_names[proj.gen.style], 12); break;
    case R_OCT: ui_num(8, y, (uint16_t)(proj.gen.octave + 1), 1); ui_fill(9, y, 11, 0); break;
    case R_MODE: ui_textn(8, y, corr_mode_names[proj.gen.mode], 12); break;
    case R_SEED:
        ui_hex(8, y, proj.gen.seed, 4);
        ui_textn(12, y, "  A:NEW ", 8);
        break;
    case R_CODE: {
        char code[12];
        uint8_t i, x;
        corr_code_encode(&proj.gen, code);
        for (i = 0; i < 12; i++) {
            x = (uint8_t)(8 + i + (i >= 4) + (i >= 8));
            ui_put(x, y, (uint8_t)(code[i] - 32));
            if (cur == R_CODE && col == i) ui_invert(x, y, 1);
        }
        ui_put(12, y, '-' - 32);
        ui_put(17, y, '-' - 32);
        break;
    }
    case R_ACT1: case R_ACT2: case R_ACT3: {
        uint8_t a = (uint8_t)(r - R_ACT1), i;
        for (i = 0; i < 2; i++) {
            uint8_t x = (uint8_t)(i * 10);
            ui_textn(x, y, act_n[a][i], 10);
            if (a == 2) {
                uint8_t on = i == 0 ? (p->lock & PLOCK_BASS) : (p->lock & PLOCK_ALL);
                ui_put((uint8_t)(x + 9), y, on ? T_LOCK : T_DOT);
            }
            if (cur == r && col == i) ui_invert(x, y, 9);
        }
        break;
    }
    default:
        ui_num(8, y, *gptr(r), 2);
        ui_put(10, y, 0);
        ui_meter(11, y, 8, *gptr(r), 15);
        ui_put(19, y, 0);
        break;
    }
}

static void draw_info(void)
{
    const pattern_t *p = &proj.pat[seq_cur];
    ui_text(0, 17, "GEN");
    ui_num0(4, 17, p->gen, 3);
    ui_text(8, 17, evo_state == EVO_READY ? "NEXT:READY  " : (proj.gen.mode == EVO_STABLE ? "NEXT:-      " : "NEXT:...    "));
}

static void draw_all(void)
{
    uint8_t r;
    ui_cls();
    ui_header("GEN");
    for (r = 0; r < N_ROWS; r++) draw_row(r);
    draw_info();
    last_pat = seq_cur;
    last_commits = evo_commits;
}

void scr_gen_enter(void) BANKED
{
    draw_all();
}

static void generate(uint8_t what)
{
    seq_backup_for_undo(seq_cur);
    corr_generate(&proj.pat[seq_cur], &proj.gen, what);
    app_pattern_edited();
    ui_msg(what == GEN_ALL ? "GENERATED BASS+DRUMS" : "GENERATED BASS");
}

static void code_digit(uint8_t i, int8_t d)
{
    uint8_t *f;
    int8_t step = d > 0 ? 1 : -1;
    if (i < 4) {
        uint8_t sh = (uint8_t)((3 - i) * 4);
        uint16_t nib = (proj.gen.seed >> sh) & 15;
        nib = (uint16_t)((nib + (uint16_t)(step + 16)) & 15);
        proj.gen.seed = (uint16_t)((proj.gen.seed & ~((uint16_t)15 << sh)) | (nib << sh));
        return;
    }
    switch (i) {
    case 4: f = &proj.gen.key; break;
    case 5: f = &proj.gen.scale; break;
    case 6: f = &proj.gen.style; break;
    case 7: f = &proj.gen.density; break;
    case 8: f = &proj.gen.sync; break;
    case 9: f = &proj.gen.accent; break;
    case 10: f = &proj.gen.slide; break;
    default: f = &proj.gen.octave; break;
    }
    {
        static const uint8_t mx[12] = { 15, 15, 15, 15, 11, SCALE_COUNT - 1, STYLE_COUNT - 1, 15, 15, 15, 15, 2 };
        *f = clampadd(*f, step, 0, mx[i]);
    }
}

void scr_gen_frame(void) BANKED
{
    uint8_t old = cur, oldc = col, r;
    int8_t d = 0;

    if (seq_cur != last_pat || evo_commits != last_commits) draw_all();

    if (!(key_held & J_SELECT)) {
        if (key_held & J_B) {
            d = ui_adjust(cur == R_SEED ? 2 : 4);
        } else {
            if (key_repeat & J_UP) cur = cur ? (uint8_t)(cur - 1) : (uint8_t)(N_ROWS - 1);
            if (key_repeat & J_DOWN) cur = (uint8_t)((cur + 1) % N_ROWS);
            if (key_repeat & (J_LEFT | J_RIGHT)) {
                int8_t dir = (key_repeat & J_RIGHT) ? 1 : -1;
                if (cur == R_CODE) col = (uint8_t)((col + 12 + dir) % 12);
                else if (cur >= R_ACT1) col = (uint8_t)(col ^ 1);
                else if (cur != R_SEED) d = dir;
            }
            if (key_pressed & J_A) {
                pattern_t *p = &proj.pat[seq_cur];
                if (cur == R_SEED) {
                    proj.gen.seed = (uint16_t)((proj.gen.seed * 25173u + 13849u) ^ ((uint16_t)DIV_REG << 8) ^ sys_time);
                    generate(GEN_ALL);
                } else if (cur == R_ACT1) {
                    generate(col ? GEN_BASS : GEN_ALL);
                } else if (cur == R_ACT2) {
                    if (col) app_undo(); else app_evolve_now();
                } else if (cur == R_ACT3) {
                    p->lock ^= col ? PLOCK_ALL : PLOCK_BASS;
                    app_pattern_edited();
                    ui_msg(col ? ((p->lock & PLOCK_ALL) ? "PATTERN LOCKED" : "PATTERN UNLOCKED")
                               : ((p->lock & PLOCK_BASS) ? "BASS LOCKED" : "BASS UNLOCKED"));
                }
                draw_all();
            }
        }
    }
    if (cur >= R_ACT1 && old < R_ACT1) col = 0;
    if (cur == R_CODE && old != R_CODE) col = 0;
    if (d) {
        if (cur == R_SEED) {
            proj.gen.seed = (uint16_t)(proj.gen.seed + (d == 2 ? 256 : d == -2 ? -256 : d));
        } else if (cur == R_CODE) {
            code_digit(col, d);
        } else if (cur <= R_MODE) {
            uint8_t *f = gptr(cur);
            *f = clampadd(*f, (gmax[cur] < 8) ? (d > 0 ? 1 : -1) : d, 0, gmax[cur]);
        }
        evo_state = EVO_IDLE;
        for (r = 0; r <= R_CODE; r++) draw_row(r);
    }
    if (old != cur || oldc != col) {
        draw_row(old);
        draw_row(cur);
    }
    draw_info();
}

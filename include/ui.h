/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * User interface core (bank 0) and screen entry points (banked).
 */
#ifndef UI_H
#define UI_H

#include <gbdk/platform.h>
#include "etch.h"
#include "gfx_tiles.h"

#define SCR_W 20
#define SCR_H 18

/* screens, in SELECT+LEFT/RIGHT order */
enum { SCR_LIVE, SCR_GRID, SCR_DRUM, SCR_SYNTH, SCR_GEN, SCR_CHAIN, SCR_SAVE, SCR_COUNT, SCR_TITLE = 0xFE };

/* ---- shadow tile map --------------------------------------------------- */
void ui_cls(void);
void ui_put(uint8_t x, uint8_t y, uint8_t tile);
uint8_t ui_get(uint8_t x, uint8_t y);
void ui_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void ui_text(uint8_t x, uint8_t y, const char *s);
void ui_textn(uint8_t x, uint8_t y, const char *s, uint8_t n);  /* pad/clip to n */
void ui_invert(uint8_t x, uint8_t y, uint8_t w);
void ui_num(uint8_t x, uint8_t y, uint16_t v, uint8_t digits);  /* space padded */
void ui_num0(uint8_t x, uint8_t y, uint16_t v, uint8_t digits); /* zero padded  */
void ui_hex(uint8_t x, uint8_t y, uint16_t v, uint8_t digits);
void ui_note(uint8_t x, uint8_t y, uint8_t note);                /* "C#2" */
void ui_meter(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max);
void ui_flush(void);
void ui_flush_all(void);
void ui_vram_tiles(uint8_t first, uint8_t count, const uint8_t *data); /* safe tile upload */

/* header (row 0) + message line (row 1) */
void ui_header(const char *name);
void ui_header_update(void);
void ui_msg(const char *s);
void ui_msg_tick(void);

/* ---- input ------------------------------------------------------------- */
extern uint8_t key_held, key_pressed, key_repeat;
void ui_input(void);
void ui_input_init(void);
#define K_DPAD (J_UP | J_DOWN | J_LEFT | J_RIGHT)

/* screens (banked) */
void scr_title_run(void) BANKED;
void scr_live_enter(void) BANKED;  void scr_live_frame(void) BANKED;
void scr_grid_enter(void) BANKED;  void scr_grid_frame(void) BANKED;
void scr_drum_enter(void) BANKED;  void scr_drum_frame(void) BANKED;
void scr_synth_enter(void) BANKED; void scr_synth_frame(void) BANKED;
void scr_gen_enter(void) BANKED;   void scr_gen_frame(void) BANKED;
void scr_chain_enter(void) BANKED; void scr_chain_frame(void) BANKED;
void scr_save_enter(void) BANKED;  void scr_save_frame(void) BANKED;

/* shared app services (src/main.c) */
extern uint8_t app_screen;
void app_redraw(void);                 /* request full redraw of current screen */
void app_pattern_edited(void);         /* user changed the current pattern     */
void app_audition(uint8_t note, uint8_t accent);
void app_fill(void);
void app_evolve_now(void);
void app_undo(void);
void app_load_track(uint8_t t);
void app_project_loaded(void);
uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi);

/* B held + D-pad adjust helper: returns delta (UP/DOWN = fine, L/R = coarse) */
int8_t ui_adjust(uint8_t coarse);

#endif

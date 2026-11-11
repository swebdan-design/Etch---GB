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

/* screens, left to right; SELECT+LEFT/RIGHT walks the row (no wrap).
 *   SYNTH  <  TRACK  <  LIVE  >  GEN  >  SONG  >  FILE                    */
enum { SCR_SYNTH, SCR_TRACK, SCR_LIVE, SCR_GEN, SCR_SONG, SCR_FILE, SCR_COUNT, SCR_TITLE = 0xFE };
extern const char * const scr_names[SCR_COUNT];

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
void ui_row1(const char *s);            /* persistent row-1 text (NULL = rule) */
void ui_msg_map(uint8_t screen);        /* flash "< LEFT [HERE] RIGHT >" */
void ui_header_update(void);
void ui_msg(const char *s);
void ui_msg_tick(void);

/* ---- input ------------------------------------------------------------- */
extern uint8_t key_held, key_pressed, key_repeat, key_released;
extern uint16_t ui_entropy;              /* mixed from button-press timing */
void ui_input(void);
/* Editing grammar (all screens):
 *   D-pad            move the cursor            (ui_nav)
 *   A (tap)          place / toggle / do        (ui_a_press on press, ui_a_tap on release)
 *   A held + L/R     value -1 / +1              (ui_edit)
 *   A held + U/D     value +coarse / -coarse    (ui_edit)
 *   B + A            delete / reset to default  (ui_clear)
 *   B + D-pad        page jump (screen specific) (ui_page)                  */
uint8_t ui_nav(void);
int8_t  ui_edit(uint8_t coarse);
uint8_t ui_a_press(void);
uint8_t ui_a_tap(void);
uint8_t ui_clear(void);
uint8_t ui_page(void);
uint8_t ui_b_tap(void);                 /* B pressed and released on its own: back */
void    ui_a_consume(void);
void ui_input_init(void);
#define K_DPAD (J_UP | J_DOWN | J_LEFT | J_RIGHT)

/* screens (banked) */
void scr_title_run(void) BANKED;
void scr_live_enter(void) BANKED;  void scr_live_frame(void) BANKED;
void scr_track_enter(void) BANKED; void scr_track_frame(void) BANKED;
void scr_synth_enter(void) BANKED; void scr_synth_frame(void) BANKED;
void scr_gen_enter(void) BANKED;   void scr_gen_frame(void) BANKED;
void scr_song_enter(void) BANKED;  void scr_song_frame(void) BANKED;
void scr_file_enter(void) BANKED;  void scr_file_frame(void) BANKED;

/* shared app services (src/main.c) */
extern uint8_t app_screen;
void app_redraw(void);                 /* request full redraw of current screen */
void app_pattern_edited(void);         /* user changed the current pattern     */
void app_audition(uint8_t note, uint8_t accent);
extern uint8_t app_last_note;          /* note placed by A on an empty step */
void app_fill(void);
void app_fill_toggle(void);             /* latched fill on/off */
uint16_t app_regen(uint8_t what);       /* GEN_ALL / GEN_BASS / GEN_DRUMS; mask of new steps */
void app_evolve_now(void);
void app_undo(void);
void app_load_track(uint8_t t);
void app_project_loaded(void);
void app_new_project(void);
void app_go_live(uint8_t play);          /* switch to LIVE (and start playing) */
uint8_t clampadd(uint8_t v, int8_t d, uint8_t lo, uint8_t hi);


#endif

/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Title screen, laid out as ETCH DS's (SERIES_SPEC.md 3.3): the wordmark,
 * the subtitle, "by Synth Centric", the version, PRESS START.  Drawn with
 * the LCD off (title tiles go straight to VRAM), then only the blinking
 * PRESS START row is touched.
 */
#pragma bank 255
#include <gbdk/platform.h>
#include "ui.h"

void scr_title_run(void) BANKED
{
    uint8_t t = 0;
    DISPLAY_OFF;
    gfx_load_logo();
    ui_cls();
    ui_text(2, 10, "by Synth Centric");
    ui_text(4, 14, "PRESS START");
    ui_flush_all();
    gfx_draw_logo(0, 2);
    gfx_draw_title_line(0, 7);           /* A GENERATIVE ACID MACHINE */
    gfx_draw_title_line(1, 8);           /* the hairline              */
    gfx_draw_title_line(2, 12);          /* VERSION / CORROSION       */
    gfx_draw_title_line(3, 17);          /* HEADPHONES RECOMMENDED    */
    DISPLAY_ON;
    while (1) {
        wait_vbl_done();
        ui_input();
        if (key_pressed & J_START) break;
        t++;
        if ((t & 31) == 0) ui_text(4, 14, "PRESS START");
        else if ((t & 31) == 24) ui_fill(4, 14, 11, 0);
        ui_flush();
    }
    DISPLAY_OFF;
    gfx_load_ui_tiles();
}

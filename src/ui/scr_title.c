/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Title screen.  Drawn with the LCD off (logo tiles go straight to VRAM),
 * then only the blinking PRESS START row is touched.
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
    ui_fill(1, 8, 18, '-' - 32);
    ui_text(2, 11, "by Synth Centric");
    ui_text(4, 14, "PRESS START");
    ui_text(0, 17, "v" ETCH_VERSION_STR);
    ui_text(14, 17, "8 TRKS");
    ui_flush_all();
    gfx_draw_logo(2, 2);
    gfx_draw_subtitle(9);
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

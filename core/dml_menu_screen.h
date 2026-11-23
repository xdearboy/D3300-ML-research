#ifndef DML_MENU_SCREEN_H
#define DML_MENU_SCREEN_H

#include "dml_draw_buffer.h"
#include "dml_menu.h"
#include "dml_camera_status.h"

/* D3300-ML Flat Cinema Settings Menu Renderer
   Strict 90-degree flat geometry, Catppuccin Mocha colors, and precise grid layout.
   Renders onto 720x480 32bpp ARGB8888 DrawBuffer. */

int dml_menu_screen_render(
    DmlDrawBufferTable *table,
    int buf_idx,
    const DmlMenu *menu,
    const DmlCameraStatus *status
);

#endif /* DML_MENU_SCREEN_H */

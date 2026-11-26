#ifndef DML_MENU_SCREEN_H
#define DML_MENU_SCREEN_H

#include "dml_draw_buffer.h"
#include "dml_menu.h"
#include "dml_camera_status.h"

/* Host-simulator menu renderer.  The 720x480 ARGB8888 surface is a project
   design choice; it is not a recovered D3300 LCD buffer format or address. */

int dml_menu_screen_render(
    DmlDrawBufferTable *table,
    int buf_idx,
    const DmlMenu *menu,
    const DmlCameraStatus *status
);

#endif /* DML_MENU_SCREEN_H */

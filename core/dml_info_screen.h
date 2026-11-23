#ifndef DML_INFO_SCREEN_H
#define DML_INFO_SCREEN_H

#include <stdint.h>
#include "dml_draw_buffer.h"
#include "dml_camera_status.h"

/* D3300-ML Custom Shooting Information Display Screen
   Renders onto the 720x480 32bpp ARGB8888 DrawBuffer (0x104A0868). */

/* Color palette constants (ARGB8888) */
#define DML_COLOR_BG            0xFF101318u /* Deep obsidian black */
#define DML_COLOR_CARD_BG       0xFF191E26u /* Dark slate card background */
#define DML_COLOR_CARD_BORDER   0xFF2B3342u /* Subtle card outline */
#define DML_COLOR_TEXT_PRIMARY  0xFFFFFFFFu /* Pure white for primary digits */
#define DML_COLOR_TEXT_MUTED    0xFF8E9AA8u /* Cool gray for labels and titles */
#define DML_COLOR_ACCENT_BLUE   0xFF0099FFu /* Electric Nikon cyan */
#define DML_COLOR_ACCENT_AMBER  0xFFFFAA00u /* Warm amber for ISO and warnings */
#define DML_COLOR_ACCENT_GREEN  0xFF00CC66u /* Emerald green for battery/status */
#define DML_COLOR_ALERT_RED     0xFFFF3344u /* Red for low battery/errors */

/* Renders the full 720x480 custom info screen into the specified draw buffer slot.
   Returns 0 on success, -1 on invalid parameter or buffer error. */
int dml_info_screen_render(DmlDrawBufferTable *table, int buf_idx, const DmlCameraStatus *status);

#endif /* DML_INFO_SCREEN_H */

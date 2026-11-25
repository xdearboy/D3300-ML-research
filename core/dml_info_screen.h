#ifndef DML_INFO_SCREEN_H
#define DML_INFO_SCREEN_H

#include <stdint.h>
#include "dml_draw_buffer.h"
#include "dml_camera_status.h"

/* Host-only visual mock-up. The 720x480 ARGB8888 target is a simulator
   choice, not a recovered D3300 framebuffer format, address, or ABI. */

/* Color palette constants (ARGB8888) — Catppuccin Mocha & d3vo.ru */
#define DML_COLOR_BG            0xFF181825u /* Catppuccin Mocha Mantle #181825 */
#define DML_COLOR_SURFACE       0xFF1E1E2Eu /* Catppuccin Mocha Base #1e1e2e */
#define DML_COLOR_CARD_BG       0xFF1E1E2Eu /* Card surface */
#define DML_COLOR_CARD_BORDER   0xFF313244u /* Surface0 subtle border #313244 */
#define DML_COLOR_DIVIDER       0xFF313244u /* Hairline divider #313244 */
#define DML_COLOR_TEXT_PRIMARY  0xFFCDD6F4u /* Mocha Text #cdd6f4 */
#define DML_COLOR_TEXT_WHITE    0xFFFFFFFFu /* Crisp white */
#define DML_COLOR_TEXT_MUTED    0xFF7F849Cu /* Muted subtext #7f849c */
#define DML_COLOR_ACCENT_MAUVE  0xFFCBA6F7u /* d3vo.ru primary accent Mauve #cba6f7 */
#define DML_COLOR_ACCENT_BLUE   0xFF89B4FAu /* Blackmagic Sapphire Blue #89b4fa */
#define DML_COLOR_D3VO_EMERALD  0xFF00BB7Fu /* d3vo.ru signature Emerald #00bb7f */
#define DML_COLOR_D3VO_ORANGE   0xFFF99C00u /* d3vo.ru signature Orange #f99c00 */
#define DML_COLOR_ALERT_RED     0xFFF38BA8u /* Mocha Red #f38ba8 */

/* Legacy aliases for backward compatibility with existing tests */
#define DML_COLOR_ACCENT_AMBER  DML_COLOR_D3VO_ORANGE
#define DML_COLOR_ACCENT_GREEN  DML_COLOR_D3VO_EMERALD

/* Renders the host mock-up into the specified host draw-buffer slot.
   Returns 0 on success, -1 on invalid parameter or buffer error. */
int dml_info_screen_render(DmlDrawBufferTable *table, int buf_idx, const DmlCameraStatus *status);

#endif /* DML_INFO_SCREEN_H */

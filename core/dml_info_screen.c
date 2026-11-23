#include "dml_info_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t x0, y0, x1, y1;
} DmlStrokeSegment;

typedef struct {
    char ch;
    uint8_t count;
    DmlStrokeSegment segments[16];
} DmlStrokeGlyph;

static const DmlStrokeGlyph k_stroke_glyphs[] = {
    {'0', 8, {{10, 15, 10, 85}, {90, 15, 90, 85}, {25, 4, 75, 4}, {25, 96, 75, 96},
              {10, 15, 25, 4}, {75, 4, 90, 15}, {10, 85, 25, 96}, {75, 96, 90, 85}}},
    {'1', 3, {{50, 4, 50, 96}, {20, 25, 50, 4}, {25, 96, 75, 96}, {0, 0, 0, 0}}},
    {'2', 6, {{10, 20, 25, 4}, {25, 4, 75, 4}, {75, 4, 90, 20}, {90, 20, 90, 45},
              {90, 45, 10, 96}, {10, 96, 90, 96}}},
    {'3', 8, {{10, 4, 90, 4}, {90, 4, 50, 46}, {50, 46, 75, 46}, {75, 46, 90, 58},
              {90, 58, 90, 82}, {90, 82, 75, 96}, {75, 96, 15, 96}, {15, 96, 10, 85}}},
    {'4', 3, {{72, 4, 72, 96}, {72, 4, 10, 68}, {10, 68, 92, 68}, {0, 0, 0, 0}}},
    {'5', 7, {{90, 4, 15, 4}, {15, 4, 15, 44}, {15, 44, 75, 44}, {75, 44, 90, 58},
              {90, 58, 90, 82}, {90, 82, 75, 96}, {75, 96, 10, 96}, {0, 0, 0, 0}}},
    {'6', 9, {{80, 4, 25, 4}, {25, 4, 10, 20}, {10, 20, 10, 80}, {10, 80, 25, 96},
              {25, 96, 75, 96}, {75, 96, 90, 82}, {90, 82, 90, 54}, {90, 54, 75, 44},
              {75, 44, 10, 44}}},
    {'7', 2, {{10, 4, 90, 4}, {90, 4, 35, 96}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {'8', 15, {{25, 4, 75, 4}, {75, 4, 90, 18}, {90, 18, 90, 34}, {90, 34, 75, 48},
               {75, 48, 25, 48}, {25, 48, 10, 34}, {10, 34, 10, 18}, {10, 18, 25, 4},
               {75, 48, 90, 62}, {90, 62, 90, 82}, {90, 82, 75, 96}, {75, 96, 25, 96},
               {25, 96, 10, 82}, {10, 82, 10, 62}, {10, 62, 25, 48}}},
    {'9', 9, {{90, 48, 25, 48}, {25, 48, 10, 38}, {10, 38, 10, 18}, {10, 18, 25, 4},
              {25, 4, 75, 4}, {75, 4, 90, 18}, {90, 18, 90, 80}, {90, 80, 75, 96},
              {75, 96, 20, 96}}},
    {'/', 1, {{20, 96, 80, 4}, {0, 0, 0, 0}}},
    {'f', 4, {{65, 4, 35, 4}, {35, 4, 25, 18}, {25, 18, 25, 96}, {10, 38, 55, 38}}},
    {'F', 3, {{15, 4, 15, 96}, {15, 4, 85, 4}, {15, 48, 65, 48}, {0, 0, 0, 0}}},
    {'B', 10, {{15, 4, 15, 96}, {15, 4, 70, 4}, {70, 4, 85, 16}, {85, 16, 85, 34},
               {85, 34, 70, 48}, {70, 48, 15, 48}, {70, 48, 88, 62}, {88, 62, 88, 84},
               {88, 84, 72, 96}, {72, 96, 15, 96}}},
    {'U', 5, {{15, 4, 15, 78}, {15, 78, 30, 96}, {30, 96, 70, 96}, {70, 96, 85, 78}, {85, 78, 85, 4}}},
    {'L', 2, {{15, 4, 15, 96}, {15, 96, 85, 96}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {'-', 1, {{15, 50, 85, 50}, {0, 0, 0, 0}}},
    {'+', 2, {{15, 50, 85, 50}, {50, 15, 50, 85}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {'"', 2, {{35, 4, 35, 26}, {65, 4, 65, 26}, {0, 0, 0, 0}, {0, 0, 0, 0}}}
};
static const size_t k_glyph_count = sizeof(k_stroke_glyphs) / sizeof(k_stroke_glyphs[0]);

static void draw_stroke_line(DmlDrawBufferTable *table, int buf_idx, int x0, int y0, int x1, int y1, int width, uint32_t color) {
    if (x0 == x1) {
        int y_min = y0 < y1 ? y0 : y1;
        int h = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
        dml_draw_buffer_fill_rect(table, buf_idx, x0 - width / 2, y_min, width, h, color);
        return;
    }
    if (y0 == y1) {
        int x_min = x0 < x1 ? x0 : x1;
        int w = (x0 < x1 ? x1 - x0 : x0 - x1) + 1;
        dml_draw_buffer_fill_rect(table, buf_idx, x_min, y0 - width / 2, w, width, color);
        return;
    }

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int cur_x = x0;
    int cur_y = y0;
    int half_w = width / 2;

    while (1) {
        dml_draw_buffer_fill_rect(table, buf_idx, cur_x - half_w, cur_y - half_w, width, width, color);
        if (cur_x == x1 && cur_y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            cur_x += sx;
        }
        if (e2 < dx) {
            err += dx;
            cur_y += sy;
        }
    }
}

static const DmlStrokeGlyph *find_glyph(char ch) {
    for (size_t i = 0; i < k_glyph_count; i++) {
        if (k_stroke_glyphs[i].ch == ch) return &k_stroke_glyphs[i];
    }
    return NULL;
}

static void draw_stroke_text(DmlDrawBufferTable *table, int buf_idx, const char *text, int x, int y, int w, int h, int stroke_w, uint32_t color) {
    if (!text) return;
    int cx = x;
    while (*text) {
        char ch = *text;
        if (ch == ' ') {
            cx += w / 2;
            text++;
            continue;
        }
        if (ch == '.') {
            dml_draw_buffer_fill_rect(table, buf_idx, cx + 4, y + h - 8, 6, 6, color);
            cx += 14;
            text++;
            continue;
        }
        int cw = (ch == '1' || ch == '/') ? (w * 3) / 4 : w;
        const DmlStrokeGlyph *glyph = find_glyph(ch);
        if (glyph) {
            for (uint8_t i = 0; i < glyph->count; i++) {
                int px0 = cx + (glyph->segments[i].x0 * cw) / 100;
                int py0 = y + (glyph->segments[i].y0 * h) / 100;
                int px1 = cx + (glyph->segments[i].x1 * cw) / 100;
                int py1 = y + (glyph->segments[i].y1 * h) / 100;
                draw_stroke_line(table, buf_idx, px0, py0, px1, py1, stroke_w, color);
            }
        }
        cx += cw + 6;
        text++;
    }
}

static void draw_minimal_card(DmlDrawBufferTable *table, int buf_idx, int x, int y, int w, int h, uint32_t fill_color, uint32_t border_color) {
    dml_draw_buffer_fill_rect(table, buf_idx, x + 2, y + 2, w - 4, h - 4, fill_color);
    dml_draw_buffer_draw_line_h(table, buf_idx, x + 2, y, w - 4, border_color);
    dml_draw_buffer_draw_line_h(table, buf_idx, x + 2, y + h - 1, w - 4, border_color);
    dml_draw_buffer_draw_line_v(table, buf_idx, x, y + 2, h - 4, border_color);
    dml_draw_buffer_draw_line_v(table, buf_idx, x + w - 1, y + 2, h - 4, border_color);
    dml_draw_buffer_fill_rect(table, buf_idx, x + 1, y + 1, 1, 1, border_color);
    dml_draw_buffer_fill_rect(table, buf_idx, x + w - 2, y + 1, 1, 1, border_color);
    dml_draw_buffer_fill_rect(table, buf_idx, x + 1, y + h - 2, 1, 1, border_color);
    dml_draw_buffer_fill_rect(table, buf_idx, x + w - 2, y + h - 2, 1, 1, border_color);
}

int dml_info_screen_render(DmlDrawBufferTable *table, int buf_idx, const DmlCameraStatus *status) {
    if (!table || buf_idx < 0 || (uint32_t)buf_idx >= DML_DRAW_BUFFER_MAX || !status) {
        return -1;
    }

    /* 1. Deep OLED Cinema Canvas */
    dml_draw_buffer_clear(table, buf_idx, DML_COLOR_BG);

    /* 2. Top Borderless Status Bar (y: 16..42) */
    /* Shooting Mode (M / A / S / P) in clean Mauve */
    const char *mode_str = dml_camera_status_mode_name(status->shooting_mode);
    dml_draw_buffer_draw_text(table, buf_idx, 28, 20, mode_str, DML_COLOR_ACCENT_MAUVE, 0);

    /* Flash Status with clean lightning bolt */
    if (status->flash_raised) {
        /* Draw lightning bolt icon */
        dml_draw_buffer_fill_rect(table, buf_idx, 62, 19, 3, 3, DML_COLOR_D3VO_ORANGE);
        dml_draw_buffer_fill_rect(table, buf_idx, 59, 22, 3, 3, DML_COLOR_D3VO_ORANGE);
        dml_draw_buffer_fill_rect(table, buf_idx, 56, 25, 7, 2, DML_COLOR_D3VO_ORANGE);
        dml_draw_buffer_fill_rect(table, buf_idx, 59, 27, 3, 3, DML_COLOR_D3VO_ORANGE);
        dml_draw_buffer_fill_rect(table, buf_idx, 57, 30, 2, 4, DML_COLOR_D3VO_ORANGE);
        dml_draw_buffer_draw_text(table, buf_idx, 72, 20, "FLASH ON", DML_COLOR_D3VO_ORANGE, 0);
    } else {
        dml_draw_buffer_draw_text(table, buf_idx, 62, 20, "FLASH OFF", DML_COLOR_TEXT_MUTED, 0);
    }

    /* 11-point focus point array (3x3 grid centered at x = 210) */
    int grid_cx = 210;
    int grid_cy = 25;
    for (int r = -1; r <= 1; r++) {
        for (int c = -1; c <= 1; c++) {
            int px = grid_cx + c * 5 - 1;
            int py = grid_cy + r * 5 - 1;
            uint32_t col = (r == 0 && c == 0) ? DML_COLOR_ACCENT_BLUE : DML_COLOR_CARD_BORDER;
            dml_draw_buffer_fill_rect(table, buf_idx, px, py, 2, 2, col);
        }
    }
    dml_draw_buffer_draw_text(table, buf_idx, 224, 20, "CENTER AF", DML_COLOR_TEXT_MUTED, 0);

    /* Status flags (AE-L, AF-L) */
    int flag_x = 340;
    if (status->exposure_locked) {
        dml_draw_buffer_draw_text(table, buf_idx, flag_x, 20, "AE-L", DML_COLOR_D3VO_ORANGE, 0);
        flag_x += 44;
    }
    if (status->focus_locked) {
        dml_draw_buffer_draw_text(table, buf_idx, flag_x, 20, "AF-L", DML_COLOR_D3VO_EMERALD, 0);
    }

    /* Right: Quality + Battery */
    const char *quality_str = dml_camera_status_quality_name(status->image_quality);
    char qual_display[32];
    snprintf(qual_display, sizeof(qual_display), "%s  24M", quality_str);
    dml_draw_buffer_draw_text(table, buf_idx, 496, 20, qual_display, DML_COLOR_TEXT_PRIMARY, 0);

    /* Battery Meter */
    int batt_x = 612;
    int batt_y = 18;
    dml_draw_buffer_draw_rect(table, buf_idx, batt_x, batt_y, 32, 15, DML_COLOR_CARD_BORDER);
    dml_draw_buffer_fill_rect(table, buf_idx, batt_x + 32, batt_y + 4, 3, 7, DML_COLOR_CARD_BORDER);
    uint8_t pct = status->battery_pct > 100 ? 100 : status->battery_pct;
    int fill_w = (26 * pct) / 100;
    if (fill_w < 1 && pct > 0) fill_w = 1;
    uint32_t batt_color = pct <= 15 ? DML_COLOR_ALERT_RED : (pct <= 35 ? DML_COLOR_D3VO_ORANGE : DML_COLOR_D3VO_EMERALD);
    dml_draw_buffer_fill_rect(table, buf_idx, batt_x + 3, batt_y + 3, fill_w, 9, batt_color);

    char batt_str[16];
    snprintf(batt_str, sizeof(batt_str), "%u%%", (unsigned int)pct);
    dml_draw_buffer_draw_text(table, buf_idx, 654, 20, batt_str, DML_COLOR_TEXT_PRIMARY, 0);

    /* Hairline divider at y = 46 */
    dml_draw_buffer_draw_line_h(table, buf_idx, 24, 46, 672, DML_COLOR_DIVIDER);

    /* 3. Hero Exposure Pillars (y: 64..226) */
    dml_draw_buffer_draw_line_v(table, buf_idx, 248, 64, 162, DML_COLOR_DIVIDER);
    dml_draw_buffer_draw_line_v(table, buf_idx, 472, 64, 162, DML_COLOR_DIVIDER);

    /* Column 1: SHUTTER */
    dml_draw_buffer_draw_text(table, buf_idx, 40, 74, "SHUTTER SPEED", DML_COLOR_TEXT_MUTED, 0);
    char shutter_str[32];
    dml_camera_status_format_shutter(status, shutter_str, sizeof(shutter_str));
    draw_stroke_text(table, buf_idx, shutter_str, 40, 106, 30, 56, 3, DML_COLOR_TEXT_PRIMARY);
    dml_draw_buffer_draw_text(table, buf_idx, 40, 192, "180.0  -  0.0040s", DML_COLOR_TEXT_MUTED, 0);

    /* Column 2: IRIS / APERTURE */
    dml_draw_buffer_draw_text(table, buf_idx, 272, 74, "APERTURE (IRIS)", DML_COLOR_TEXT_MUTED, 0);
    char aperture_str[32];
    dml_camera_status_format_aperture(status, aperture_str, sizeof(aperture_str));
    char iris_display[32];
    if (aperture_str[0] == 'F') {
        snprintf(iris_display, sizeof(iris_display), "f %s", aperture_str + 1);
    } else {
        snprintf(iris_display, sizeof(iris_display), "%s", aperture_str);
    }
    draw_stroke_text(table, buf_idx, iris_display, 272, 106, 30, 56, 3, DML_COLOR_TEXT_PRIMARY);
    dml_draw_buffer_draw_text(table, buf_idx, 272, 192, "NIKKOR DX 35mm F/1.8", DML_COLOR_TEXT_MUTED, 0);

    /* Column 3: ISO */
    dml_draw_buffer_draw_text(table, buf_idx, 496, 74, "ISO SENSITIVITY", DML_COLOR_TEXT_MUTED, 0);
    char iso_str[32];
    dml_camera_status_format_iso(status, iso_str, sizeof(iso_str));
    uint32_t iso_color = status->is_auto_iso ? DML_COLOR_ACCENT_MAUVE : DML_COLOR_D3VO_ORANGE;
    draw_stroke_text(table, buf_idx, iso_str, 496, 106, 30, 56, 3, iso_color);
    dml_draw_buffer_draw_text(table, buf_idx, 496, 192, status->is_auto_iso ? "AUTO ISO ACTIVE" : "NATIVE RANGE 100-6400", DML_COLOR_TEXT_MUTED, 0);

    /* Hairline Divider at y = 240 */
    dml_draw_buffer_draw_line_h(table, buf_idx, 24, 240, 672, DML_COLOR_DIVIDER);

    /* 4. Exposure Compensation & Light Meter (y: 254..318) */
    dml_draw_buffer_draw_text(table, buf_idx, 40, 254, "EXPOSURE METER / COMPENSATION", DML_COLOR_TEXT_MUTED, 0);

    char ev_val_str[32];
    dml_camera_status_format_ev(status, ev_val_str, sizeof(ev_val_str));
    char ev_full[48];
    snprintf(ev_full, sizeof(ev_full), "%s EV", ev_val_str);
    dml_draw_buffer_draw_text(table, buf_idx, 630, 254, ev_full, DML_COLOR_D3VO_EMERALD, 0);

    int center_x = 360;
    int axis_y = 290;
    dml_draw_buffer_draw_line_h(table, buf_idx, center_x - 240, axis_y, 480, DML_COLOR_DIVIDER);

    const char *ev_labels[] = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
    for (int s = -9; s <= 9; s++) {
        int tx = center_x + s * 26;
        if (s % 3 == 0) {
            dml_draw_buffer_draw_line_v(table, buf_idx, tx, axis_y - 8, 17, DML_COLOR_TEXT_MUTED);
            int idx = (s / 3) + 3;
            if (idx >= 0 && idx < 7) {
                int lbl_len = (int)strlen(ev_labels[idx]);
                dml_draw_buffer_draw_text(table, buf_idx, tx - (lbl_len * 8) / 2, axis_y + 12, ev_labels[idx], DML_COLOR_TEXT_MUTED, 0);
            }
        } else {
            dml_draw_buffer_draw_line_v(table, buf_idx, tx, axis_y - 4, 9, DML_COLOR_DIVIDER);
        }
    }

    /* Active EV Pointer (triangle) */
    int8_t clamped_ev = status->ev_compensation_third_stops;
    if (clamped_ev < -9) clamped_ev = -9;
    if (clamped_ev > 9) clamped_ev = 9;
    int ptr_x = center_x + clamped_ev * 26;
    for (int row = 0; row < 8; row++) {
        dml_draw_buffer_fill_rect(table, buf_idx, ptr_x - (7 - row), axis_y - 12 + row, (7 - row) * 2 + 1, 1, DML_COLOR_D3VO_EMERALD);
    }

    /* 5. Minimal Bottom Cards (y: 344..420) */
    const struct {
        const char *title;
        const char *val;
        const char *sub;
        uint32_t color;
    } bottom_cards[] = {
        {"FOCUS MODE", dml_camera_status_af_name(status->af_mode), "Dynamic 11-point", DML_COLOR_TEXT_PRIMARY},
        {"METERING", dml_camera_status_metering_name(status->metering_mode), "420-pixel RGB sensor", DML_COLOR_TEXT_PRIMARY},
        {"PICTURE & WB", "Neutral [NL]", "Auto AWB [5600K]", DML_COLOR_TEXT_PRIMARY},
        {"STORAGE / BUF", "", "SD SanDisk [OK] - r12", DML_COLOR_D3VO_EMERALD}
    };

    char rem_str[32];
    snprintf(rem_str, sizeof(rem_str), "%u REM", (unsigned int)status->shots_remaining);

    int cw = 158;
    int gap = 14;
    for (int i = 0; i < 4; i++) {
        int cx = 24 + i * (cw + gap);
        draw_minimal_card(table, buf_idx, cx, 344, cw, 76, DML_COLOR_SURFACE, DML_COLOR_CARD_BORDER);
        dml_draw_buffer_draw_text(table, buf_idx, cx + 12, 354, bottom_cards[i].title, DML_COLOR_TEXT_MUTED, 0);
        const char *display_val = (i == 3) ? rem_str : bottom_cards[i].val;
        dml_draw_buffer_draw_text(table, buf_idx, cx + 12, 374, display_val, bottom_cards[i].color, 0);
        dml_draw_buffer_draw_text(table, buf_idx, cx + 12, 398, bottom_cards[i].sub, DML_COLOR_TEXT_MUTED, 0);
    }

    /* 6. Minimal Footer (y: 438..468) */
    dml_draw_buffer_draw_line_h(table, buf_idx, 24, 438, 672, DML_COLOR_DIVIDER);
    dml_draw_buffer_draw_text(table, buf_idx, 24, 450, "D3300-ML  -  CINEMA MONITOR (EXPEED 4 / Milbeaut)", DML_COLOR_TEXT_MUTED, 0);
    dml_draw_buffer_draw_text(table, buf_idx, 490, 450, "[INFO] TOGGLE VIEW    [MENU] ML OSD", DML_COLOR_TEXT_PRIMARY, 0);

    return 0;
}

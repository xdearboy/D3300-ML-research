#include "dml_menu_screen.h"
#include <stdio.h>
#include <string.h>

/* Color Tokens (ARGB8888) — Catppuccin Mocha + Cinema OS */
#define COLOR_MANTLE   0xFF181825u
#define COLOR_BASE     0xFF1E1E2Eu
#define COLOR_SURF0    0xFF313244u
#define COLOR_SURF1    0xFF45475Au
#define COLOR_TEXT     0xFFCDD6F4u
#define COLOR_WHITE    0xFFFFFFFFu
#define COLOR_MUTED    0xFF7F849Cu
#define COLOR_SUB      0xFF6C7086u
#define COLOR_MAUVE    0xFFCBA6F7u
#define COLOR_EMERALD  0xFF00BB7Fu
#define COLOR_ORANGE   0xFFF99C00u
#define COLOR_SEL_BG   0xFF222336u
#define COLOR_ROW_ALT  0xFF1B1B2Au
#define COLOR_HDR_BG   0xFF13131Du
#define COLOR_BOX_BG   0xFF14141Eu

static void draw_tab(
    DmlDrawBufferTable *table,
    int buf_idx,
    int tx,
    int ty,
    int tw,
    const char *name,
    int is_active
) {
    if (is_active) {
        dml_draw_buffer_fill_rect(table, buf_idx, tx, ty, tw, 44, COLOR_BASE);
        /* Top and bottom 2px emerald accent lines */
        dml_draw_buffer_fill_rect(table, buf_idx, tx, ty, tw, 2, COLOR_EMERALD);
        dml_draw_buffer_fill_rect(table, buf_idx, tx, ty + 42, tw, 2, COLOR_EMERALD);
        int text_w = (int)strlen(name) * 8;
        int text_x = tx + (tw - text_w) / 2;
        dml_draw_buffer_draw_text(table, buf_idx, text_x, ty + 18, name, COLOR_WHITE, 0x00000000u);
    } else {
        int text_w = (int)strlen(name) * 8;
        int text_x = tx + (tw - text_w) / 2;
        dml_draw_buffer_draw_text(table, buf_idx, text_x, ty + 18, name, COLOR_MUTED, 0x00000000u);
    }
    /* Hairline vertical divider between tabs */
    dml_draw_buffer_draw_line_v(table, buf_idx, tx + tw, ty + 10, 24, 0xFF252538u);
}

static void draw_battery(
    DmlDrawBufferTable *table,
    int buf_idx,
    int x,
    int y,
    uint8_t pct
) {
    char b_str[16];
    snprintf(b_str, sizeof(b_str), "%u%%", pct);
    dml_draw_buffer_draw_text(table, buf_idx, x - 35, y + 2, b_str, COLOR_TEXT, 0x00000000u);

    /* Flat sharp battery body: 40x16 px */
    dml_draw_buffer_draw_rect(table, buf_idx, x, y, 40, 16, COLOR_SURF1);
    
    int fill_w = ((int)pct * 34) / 100;
    if (fill_w > 0) {
        uint32_t fill_color = (pct <= 15) ? 0xFFF38BA8u : COLOR_EMERALD;
        dml_draw_buffer_fill_rect(table, buf_idx, x + 3, y + 3, fill_w, 10, fill_color);
    }
    /* Flat terminal nipple */
    dml_draw_buffer_fill_rect(table, buf_idx, x + 40, y + 4, 3, 8, COLOR_SURF1);
}

int dml_menu_screen_render(
    DmlDrawBufferTable *table,
    int buf_idx,
    const DmlMenu *menu,
    const DmlCameraStatus *status
) {
    if (!table || buf_idx < 0 || !menu) {
        return -1;
    }

    if (!menu->visible) {
        if (menu->is_holding_menu && menu->hold_progress > 0) {
            /* Render sleek Cinema hold progress overlay */
            int pill_w = 260;
            int pill_h = 44;
            int pill_x = (720 - pill_w) / 2;
            int pill_y = 416;

            dml_draw_buffer_fill_rect(table, buf_idx, pill_x, pill_y, pill_w, pill_h, COLOR_HDR_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, pill_x, pill_y, pill_w, pill_h, COLOR_SURF0);
            dml_draw_buffer_draw_text(table, buf_idx, pill_x + 14, pill_y + 8, "D3300-ML CINEMA OS", COLOR_MAUVE, 0x00000000u);

            char pct_str[8];
            snprintf(pct_str, sizeof(pct_str), "%u%%", (unsigned int)menu->hold_progress);
            dml_draw_buffer_draw_text(table, buf_idx, pill_x + pill_w - 44, pill_y + 8, pct_str, COLOR_TEXT, 0x00000000u);

            /* Progress bar track */
            int bar_x = pill_x + 14;
            int bar_y = pill_y + 26;
            int bar_w = pill_w - 28;
            int bar_h = 6;
            dml_draw_buffer_fill_rect(table, buf_idx, bar_x, bar_y, bar_w, bar_h, COLOR_SEL_BG);
            int fill_w = ((int)menu->hold_progress * bar_w) / 100;
            if (fill_w > 0) {
                dml_draw_buffer_fill_rect(table, buf_idx, bar_x, bar_y, fill_w, bar_h, COLOR_EMERALD);
            }
        }
        return 0;
    }

    /* Animation Progress (0..100) */
    int anim_p = (menu->anim_progress > 0) ? (int)menu->anim_progress : 100;
    if (anim_p > 100) anim_p = 100;

    int hdr_offset_y = (100 - anim_p) * 44 / 100;
    int hdr_y = -hdr_offset_y;

    /* 1. Base Canvas Clear */
    dml_draw_buffer_clear(table, buf_idx, COLOR_MANTLE);

    /* 2. Top Header Bar (0..44 px with slide-down) */
    dml_draw_buffer_fill_rect(table, buf_idx, 0, hdr_y, 720, 44, COLOR_HDR_BG);
    dml_draw_buffer_draw_line_h(table, buf_idx, 0, hdr_y + 44, 720, COLOR_SURF0);

    /* Brand Logo */
    dml_draw_buffer_draw_text(table, buf_idx, 20, hdr_y + 18, "D3300-ML", COLOR_MAUVE, 0x00000000u);

    /* Category Tabs */
    static const char *kTabNames[DML_TAB_COUNT] = {
        "SHOOT", "EXPO", "OVERLAY", "MOVIE", "SYSTEM"
    };
    int tab_start_x = 160;
    int tab_w = 80;

    for (int t = 0; t < (int)DML_TAB_COUNT; ++t) {
        int is_act = ((size_t)t == menu->active_tab);
        draw_tab(table, buf_idx, tab_start_x + t * tab_w, hdr_y, tab_w, kTabNames[t], is_act);
    }

    /* Battery Meter */
    uint8_t bat_pct = status ? status->battery_pct : 84u;
    draw_battery(table, buf_idx, 720 - 75, hdr_y + 14, bat_pct);

    /* 3. Settings Items Rows (y: 45..435, up to 6 rows, height 65px each) */
    size_t tab_count = dml_menu_tab_item_count(menu, (DmlMenuTab)menu->active_tab);
    size_t effective_count = (tab_count > 0) ? tab_count : menu->count;
    if (effective_count > 6) effective_count = 6;

    int row_h = 65;
    int start_y = 45;
    int right_margin = 720 - 24;

    for (size_t k = 0; k < effective_count; ++k) {
        const DmlMenuItem *it = NULL;
        if (tab_count > 0) {
            it = dml_menu_get_tab_item(menu, (DmlMenuTab)menu->active_tab, k);
        } else {
            it = &menu->items[k];
        }
        if (!it) continue;

        int y = start_y + (int)k * row_h;
        int is_sel = (k == menu->selected);

        /* Staggered entrance animation per row */
        int row_delay = (int)k * 6;
        int row_p = (anim_p > row_delay) ? ((anim_p - row_delay) * 100 / (100 - row_delay)) : 0;
        if (row_p > 100) row_p = 100;
        int slide_x = (100 - row_p) * 30 / 100;

        /* Row Background */
        if (is_sel) {
            dml_draw_buffer_fill_rect(table, buf_idx, 0, y, 720, row_h, COLOR_SEL_BG);
            /* Sharp 4px vertical accent stripe on left edge (Emerald) */
            dml_draw_buffer_fill_rect(table, buf_idx, 0, y, 4, row_h, COLOR_EMERALD);
        } else {
            uint32_t bg = (k % 2 == 0) ? COLOR_BASE : COLOR_ROW_ALT;
            dml_draw_buffer_fill_rect(table, buf_idx, 0, y, 720, row_h, bg);
        }

        /* 1px hairline horizontal divider below each row */
        dml_draw_buffer_draw_line_h(table, buf_idx, 0, y + row_h - 1, 720, COLOR_SURF0);

        /* Left text */
        uint32_t title_color = is_sel ? COLOR_WHITE : COLOR_TEXT;
        dml_draw_buffer_draw_text(table, buf_idx, 24 - slide_x, y + 15, it->label ? it->label : "", title_color, 0x00000000u);

        uint32_t sub_color = is_sel ? COLOR_MUTED : COLOR_SUB;
        dml_draw_buffer_draw_text(table, buf_idx, 24 - slide_x, y + 37, it->desc ? it->desc : "", sub_color, 0x00000000u);

        /* Right Controls (Flat 90-degree boxes, strictly right-aligned to right_margin) */
        int box_y = y + 17;
        int box_h = 30;

        if (it->has_val && it->is_on && it->value_text && it->value_text[0] != '\0') {
            /* Combined value box + ON badge */
            int val_w = 70;
            int val_x = right_margin - 60 - val_w - 8;
            dml_draw_buffer_fill_rect(table, buf_idx, val_x, box_y, val_w, box_h, COLOR_BOX_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, val_x, box_y, val_w, box_h, COLOR_SURF0);

            int tw = (int)strlen(it->value_text) * 8;
            int tx = val_x + (val_w - tw) / 2;
            dml_draw_buffer_draw_text(table, buf_idx, tx, box_y + 9, it->value_text, COLOR_ORANGE, 0x00000000u);

            int on_x = right_margin - 60;
            dml_draw_buffer_fill_rect(table, buf_idx, on_x, box_y, 60, box_h, COLOR_EMERALD);
            int tw_on = 2 * 8;
            dml_draw_buffer_draw_text(table, buf_idx, on_x + (60 - tw_on) / 2, box_y + 9, "ON", COLOR_WHITE, 0x00000000u);

        } else if (it->is_on || (it->status_text && strcmp(it->status_text, "ON") == 0)) {
            /* Standalone ON badge */
            int on_x = right_margin - 60;
            dml_draw_buffer_fill_rect(table, buf_idx, on_x, box_y, 60, box_h, COLOR_EMERALD);
            int tw_on = 2 * 8;
            dml_draw_buffer_draw_text(table, buf_idx, on_x + (60 - tw_on) / 2, box_y + 9, "ON", COLOR_WHITE, 0x00000000u);

        } else if (it->status_text && strcmp(it->status_text, "OFF") == 0) {
            /* Flat OFF badge */
            int off_x = right_margin - 60;
            dml_draw_buffer_fill_rect(table, buf_idx, off_x, box_y, 60, box_h, COLOR_BOX_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, off_x, box_y, 60, box_h, COLOR_SURF0);
            int tw_off = 3 * 8;
            dml_draw_buffer_draw_text(table, buf_idx, off_x + (60 - tw_off) / 2, box_y + 9, "OFF", COLOR_MUTED, 0x00000000u);

        } else if (it->value_text && it->value_text[0] != '\0') {
            /* Standalone value box (e.g. 5.0 FPS, 10 SEC, NEVER) */
            int val_w = 84;
            int val_x = right_margin - val_w;
            dml_draw_buffer_fill_rect(table, buf_idx, val_x, box_y, val_w, box_h, COLOR_BOX_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, val_x, box_y, val_w, box_h, COLOR_SURF0);
            int tw = (int)strlen(it->value_text) * 8;
            int tx = val_x + (val_w - tw) / 2;
            dml_draw_buffer_draw_text(table, buf_idx, tx, box_y + 9, it->value_text, COLOR_MAUVE, 0x00000000u);

        } else if (it->status_text && it->status_text[0] != '\0') {
            int stat_w = 60;
            int stat_x = right_margin - stat_w;
            dml_draw_buffer_fill_rect(table, buf_idx, stat_x, box_y, stat_w, box_h, COLOR_BOX_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, stat_x, box_y, stat_w, box_h, COLOR_SURF0);
            int tw = (int)strlen(it->status_text) * 8;
            int tx = stat_x + (stat_w - tw) / 2;
            dml_draw_buffer_draw_text(table, buf_idx, tx, box_y + 9, it->status_text, COLOR_TEXT, 0x00000000u);
        } else {
            /* Clean action indicator */
            int act_w = 32;
            int act_x = right_margin - act_w;
            dml_draw_buffer_fill_rect(table, buf_idx, act_x, box_y, act_w, box_h, COLOR_BOX_BG);
            dml_draw_buffer_draw_rect(table, buf_idx, act_x, box_y, act_w, box_h, COLOR_SURF0);
            dml_draw_buffer_draw_text(table, buf_idx, act_x + 12, box_y + 9, ">", COLOR_MUTED, 0x00000000u);
        }
    }

    /* 4. Bottom Footer Bar (436..480 px with slide-up) */
    int ftr_offset_y = (100 - anim_p) * 44 / 100;
    int ftr_y = 436 + ftr_offset_y;

    dml_draw_buffer_fill_rect(table, buf_idx, 0, ftr_y, 720, 44, 0xFF12121Bu);
    dml_draw_buffer_draw_line_h(table, buf_idx, 0, ftr_y, 720, COLOR_SURF0);

    /* Navigation Hints with Color Coding */
    dml_draw_buffer_draw_text(table, buf_idx, 24, ftr_y + 17, "[UP/DOWN]", COLOR_MAUVE, 0x00000000u);
    dml_draw_buffer_draw_text(table, buf_idx, 104, ftr_y + 17, "NAVIGATE", COLOR_MUTED, 0x00000000u);

    dml_draw_buffer_draw_text(table, buf_idx, 184, ftr_y + 17, "[OK]", COLOR_EMERALD, 0x00000000u);
    dml_draw_buffer_draw_text(table, buf_idx, 222, ftr_y + 17, "TOGGLE/EDIT", COLOR_MUTED, 0x00000000u);

    dml_draw_buffer_draw_text(table, buf_idx, 330, ftr_y + 17, "[LEFT/RIGHT]", COLOR_ORANGE, 0x00000000u);
    dml_draw_buffer_draw_text(table, buf_idx, 432, ftr_y + 17, "SWITCH TAB", COLOR_MUTED, 0x00000000u);

    dml_draw_buffer_draw_text(table, buf_idx, 580, ftr_y + 17, "[MENU]", COLOR_MAUVE, 0x00000000u);
    dml_draw_buffer_draw_text(table, buf_idx, 634, ftr_y + 17, "RETURN", COLOR_MUTED, 0x00000000u);

    return 0;
}

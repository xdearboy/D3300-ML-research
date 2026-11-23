#include "dml_info_screen.h"
#include <stdio.h>
#include <string.h>

static void draw_rounded_card(DmlDrawBufferTable *table, int buf_idx, int x, int y, int w, int h) {
    dml_draw_buffer_fill_rect(table, buf_idx, x, y, w, h, DML_COLOR_CARD_BG);
    dml_draw_buffer_draw_rect(table, buf_idx, x, y, w, h, DML_COLOR_CARD_BORDER);
}

static void draw_battery_icon(DmlDrawBufferTable *table, int buf_idx, int x, int y, uint8_t pct, uint8_t warning) {
    /* Battery body: 36 x 18 px */
    int body_w = 36;
    int body_h = 18;
    dml_draw_buffer_draw_rect(table, buf_idx, x, y, body_w, body_h, DML_COLOR_CARD_BORDER);

    /* Positive terminal nipple: 3 x 8 px */
    dml_draw_buffer_fill_rect(table, buf_idx, x + body_w, y + 5, 3, 8, DML_COLOR_CARD_BORDER);

    /* Fill level */
    int inner_w = body_w - 4;
    int inner_h = body_h - 4;
    if (pct > 100) pct = 100;
    int fill_w = (int)((inner_w * pct) / 100);
    if (fill_w < 1 && pct > 0) fill_w = 1;

    uint32_t fill_color = DML_COLOR_ACCENT_GREEN;
    if (warning == 2 || pct <= 15) {
        fill_color = DML_COLOR_ALERT_RED;
    } else if (warning == 1 || pct <= 35) {
        fill_color = DML_COLOR_ACCENT_AMBER;
    }

    if (fill_w > 0) {
        dml_draw_buffer_fill_rect(table, buf_idx, x + 2, y + 2, fill_w, inner_h, fill_color);
    }
}

int dml_info_screen_render(DmlDrawBufferTable *table, int buf_idx, const DmlCameraStatus *status) {
    if (!table || buf_idx < 0 || (uint32_t)buf_idx >= DML_DRAW_BUFFER_MAX || !status) {
        return -1;
    }

    /* 1. Clear display background */
    dml_draw_buffer_clear(table, buf_idx, DML_COLOR_BG);

    /* 2. Top Header Bar (y: 10..46) */
    /* Shooting mode pill */
    const char *mode_str = dml_camera_status_mode_name(status->shooting_mode);
    int mode_pill_w = (int)strlen(mode_str) > 1 ? 72 : 48;
    dml_draw_buffer_fill_rect(table, buf_idx, 16, 12, mode_pill_w, 28, DML_COLOR_ACCENT_BLUE);
    dml_draw_buffer_draw_text_scaled(table, buf_idx, 16 + (mode_pill_w - (int)strlen(mode_str) * 16) / 2, 18, mode_str, DML_COLOR_TEXT_PRIMARY, 0, 2);

    /* Status flags (AE-L, AF-L, Flash) */
    int flag_x = 16 + mode_pill_w + 14;
    if (status->exposure_locked) {
        dml_draw_buffer_fill_rect(table, buf_idx, flag_x, 14, 52, 24, DML_COLOR_CARD_BG);
        dml_draw_buffer_draw_rect(table, buf_idx, flag_x, 14, 52, 24, DML_COLOR_ACCENT_AMBER);
        dml_draw_buffer_draw_text(table, buf_idx, flag_x + 8, 22, "AE-L", DML_COLOR_ACCENT_AMBER, 0);
        flag_x += 60;
    }
    if (status->focus_locked) {
        dml_draw_buffer_fill_rect(table, buf_idx, flag_x, 14, 52, 24, DML_COLOR_CARD_BG);
        dml_draw_buffer_draw_rect(table, buf_idx, flag_x, 14, 52, 24, DML_COLOR_ACCENT_GREEN);
        dml_draw_buffer_draw_text(table, buf_idx, flag_x + 8, 22, "AF-L", DML_COLOR_ACCENT_GREEN, 0);
        flag_x += 60;
    }
    if (status->flash_ready) {
        dml_draw_buffer_draw_text(table, buf_idx, flag_x, 22, "[FLASH]", DML_COLOR_ACCENT_AMBER, 0);
    }

    /* Center title */
    dml_draw_buffer_draw_text(table, buf_idx, 290, 22, "D3300-ML INFO", DML_COLOR_TEXT_MUTED, 0);

    /* Right header: Quality badge + Battery */
    const char *quality_str = dml_camera_status_quality_name(status->image_quality);
    dml_draw_buffer_fill_rect(table, buf_idx, 492, 14, 60, 24, DML_COLOR_CARD_BG);
    dml_draw_buffer_draw_rect(table, buf_idx, 492, 14, 60, 24, DML_COLOR_CARD_BORDER);
    dml_draw_buffer_draw_text(table, buf_idx, 498, 22, quality_str, DML_COLOR_TEXT_PRIMARY, 0);

    draw_battery_icon(table, buf_idx, 570, 17, status->battery_pct, status->battery_warning);
    char batt_str[16];
    snprintf(batt_str, sizeof(batt_str), "%u%%", (unsigned int)status->battery_pct);
    dml_draw_buffer_draw_text(table, buf_idx, 616, 22, batt_str, DML_COLOR_TEXT_PRIMARY, 0);

    /* Header separator line */
    dml_draw_buffer_draw_line_h(table, buf_idx, 16, 48, 688, DML_COLOR_CARD_BORDER);

    /* 3. Hero Exposure Cluster (y: 56..228, 3 cards: Shutter, Aperture, ISO) */
    char shutter_str[32];
    dml_camera_status_format_shutter(status, shutter_str, sizeof(shutter_str));

    char aperture_str[32];
    dml_camera_status_format_aperture(status, aperture_str, sizeof(aperture_str));

    char iso_str[32];
    dml_camera_status_format_iso(status, iso_str, sizeof(iso_str));

    int card_w = 220;
    int card_h = 172;

    /* Card 1: SHUTTER */
    int card1_x = 16;
    int card_y = 56;
    draw_rounded_card(table, buf_idx, card1_x, card_y, card_w, card_h);
    dml_draw_buffer_draw_text(table, buf_idx, card1_x + 16, card_y + 16, "SHUTTER SPEED", DML_COLOR_TEXT_MUTED, 0);
    int shutter_len = (int)strlen(shutter_str);
    int shutter_px_w = shutter_len * 32;
    int shutter_x = card1_x + (card_w - shutter_px_w) / 2;
    if (shutter_x < card1_x + 10) shutter_x = card1_x + 10;
    dml_draw_buffer_draw_text_scaled(table, buf_idx, shutter_x, card_y + 60, shutter_str, DML_COLOR_TEXT_PRIMARY, 0, 4);
    dml_draw_buffer_draw_text(table, buf_idx, card1_x + 16, card_y + 144, "SECONDS", DML_COLOR_TEXT_MUTED, 0);

    /* Card 2: APERTURE */
    int card2_x = 250;
    draw_rounded_card(table, buf_idx, card2_x, card_y, card_w, card_h);
    dml_draw_buffer_draw_text(table, buf_idx, card2_x + 16, card_y + 16, "APERTURE", DML_COLOR_TEXT_MUTED, 0);
    int aperture_len = (int)strlen(aperture_str);
    int aperture_px_w = aperture_len * 32;
    int aperture_x = card2_x + (card_w - aperture_px_w) / 2;
    if (aperture_x < card2_x + 10) aperture_x = card2_x + 10;
    dml_draw_buffer_draw_text_scaled(table, buf_idx, aperture_x, card_y + 60, aperture_str, DML_COLOR_TEXT_PRIMARY, 0, 4);
    dml_draw_buffer_draw_text(table, buf_idx, card2_x + 16, card_y + 144, "F-NUMBER", DML_COLOR_TEXT_MUTED, 0);

    /* Card 3: ISO */
    int card3_x = 484;
    draw_rounded_card(table, buf_idx, card3_x, card_y, card_w, card_h);
    dml_draw_buffer_draw_text(table, buf_idx, card3_x + 16, card_y + 16, "ISO SENSITIVITY", DML_COLOR_TEXT_MUTED, 0);
    int iso_len = (int)strlen(iso_str);
    int iso_px_w = iso_len * 32;
    int iso_x = card3_x + (card_w - iso_px_w) / 2;
    if (iso_x < card3_x + 10) iso_x = card3_x + 10;
    uint32_t iso_color = status->is_auto_iso ? DML_COLOR_ACCENT_AMBER : DML_COLOR_TEXT_PRIMARY;
    dml_draw_buffer_draw_text_scaled(table, buf_idx, iso_x, card_y + 60, iso_str, iso_color, 0, 4);
    dml_draw_buffer_draw_text(table, buf_idx, card3_x + 16, card_y + 144, status->is_auto_iso ? "AUTO ISO" : "MANUAL ISO", DML_COLOR_TEXT_MUTED, 0);

    /* 4. Analog EV Compensation Bar (y: 236..312, height: 76) */
    int ev_y = 236;
    draw_rounded_card(table, buf_idx, 16, ev_y, 688, 76);
    dml_draw_buffer_draw_text(table, buf_idx, 28, ev_y + 12, "EXPOSURE COMPENSATION", DML_COLOR_TEXT_MUTED, 0);

    char ev_val_str[32];
    dml_camera_status_format_ev(status, ev_val_str, sizeof(ev_val_str));
    char ev_full_str[48];
    snprintf(ev_full_str, sizeof(ev_full_str), "%s EV", ev_val_str);
    dml_draw_buffer_draw_text(table, buf_idx, 600, ev_y + 12, ev_full_str, DML_COLOR_ACCENT_BLUE, 0);

    /* Scale baseline at y = ev_y + 44 */
    int axis_y = ev_y + 44;
    int center_x = 360;
    int step_px = 30; /* 30 pixels per 1/3 EV => 90 px per 1 EV */
    dml_draw_buffer_draw_line_h(table, buf_idx, center_x - 9 * step_px, axis_y, 18 * step_px, DML_COLOR_CARD_BORDER);

    /* Draw ticks from -9 to +9 (representing -3.0 EV to +3.0 EV) */
    const char *ev_labels[] = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
    for (int step = -9; step <= 9; step++) {
        int tx = center_x + step * step_px;
        if (step % 3 == 0) {
            /* Major 1.0 EV tick */
            dml_draw_buffer_draw_line_v(table, buf_idx, tx, axis_y - 8, 16, DML_COLOR_TEXT_MUTED);
            int label_idx = (step / 3) + 3;
            if (label_idx >= 0 && label_idx < 7) {
                int lbl_len = (int)strlen(ev_labels[label_idx]);
                dml_draw_buffer_draw_text(table, buf_idx, tx - (lbl_len * 8) / 2, axis_y + 12, ev_labels[label_idx], DML_COLOR_TEXT_MUTED, 0);
            }
        } else {
            /* Minor 1/3 EV tick */
            dml_draw_buffer_draw_line_v(table, buf_idx, tx, axis_y - 4, 8, DML_COLOR_CARD_BORDER);
        }
    }

    /* Active EV Pointer */
    int8_t clamped_ev_step = status->ev_compensation_third_stops;
    if (clamped_ev_step < -9) clamped_ev_step = -9;
    if (clamped_ev_step > 9) clamped_ev_step = 9;
    int ptr_x = center_x + clamped_ev_step * step_px;
    /* Draw diamond / triangle indicator for current EV */
    dml_draw_buffer_fill_rect(table, buf_idx, ptr_x - 4, axis_y - 14, 9, 6, DML_COLOR_ACCENT_BLUE);
    dml_draw_buffer_fill_rect(table, buf_idx, ptr_x - 2, axis_y - 8, 5, 4, DML_COLOR_ACCENT_BLUE);
    dml_draw_buffer_fill_rect(table, buf_idx, ptr_x, axis_y - 4, 1, 3, DML_COLOR_ACCENT_BLUE);

    /* 5. Secondary Status Cards (y: 320..424, 4 cards: AF, Metering, WB, Remaining) */
    int card4_w = 164;
    int card4_h = 104;
    int row2_y = 320;

    /* Card 1: AF Mode */
    int sc1_x = 16;
    draw_rounded_card(table, buf_idx, sc1_x, row2_y, card4_w, card4_h);
    dml_draw_buffer_draw_text(table, buf_idx, sc1_x + 12, row2_y + 12, "AF MODE", DML_COLOR_TEXT_MUTED, 0);
    const char *af_str = dml_camera_status_af_name(status->af_mode);
    dml_draw_buffer_draw_text_scaled(table, buf_idx, sc1_x + 12, row2_y + 36, af_str, DML_COLOR_TEXT_PRIMARY, 0, 2);
    dml_draw_buffer_draw_text(table, buf_idx, sc1_x + 12, row2_y + 76, "Single Servo", DML_COLOR_TEXT_MUTED, 0);

    /* Card 2: Metering */
    int sc2_x = 190;
    draw_rounded_card(table, buf_idx, sc2_x, row2_y, card4_w, card4_h);
    dml_draw_buffer_draw_text(table, buf_idx, sc2_x + 12, row2_y + 12, "METERING", DML_COLOR_TEXT_MUTED, 0);
    const char *metering_str = dml_camera_status_metering_name(status->metering_mode);
    dml_draw_buffer_draw_text_scaled(table, buf_idx, sc2_x + 12, row2_y + 36, metering_str, DML_COLOR_TEXT_PRIMARY, 0, 2);
    dml_draw_buffer_draw_text(table, buf_idx, sc2_x + 12, row2_y + 76, "3D Color RGB", DML_COLOR_TEXT_MUTED, 0);

    /* Card 3: White Balance */
    int sc3_x = 364;
    draw_rounded_card(table, buf_idx, sc3_x, row2_y, card4_w, card4_h);
    dml_draw_buffer_draw_text(table, buf_idx, sc3_x + 12, row2_y + 12, "WHITE BAL", DML_COLOR_TEXT_MUTED, 0);
    const char *wb_str = dml_camera_status_wb_name(status->white_balance);
    dml_draw_buffer_draw_text_scaled(table, buf_idx, sc3_x + 12, row2_y + 36, wb_str, DML_COLOR_TEXT_PRIMARY, 0, 2);
    dml_draw_buffer_draw_text(table, buf_idx, sc3_x + 12, row2_y + 76, "Auto [Keep W]", DML_COLOR_TEXT_MUTED, 0);

    /* Card 4: Remaining Shots */
    int sc4_x = 538;
    draw_rounded_card(table, buf_idx, sc4_x, row2_y, card4_w, card4_h);
    dml_draw_buffer_draw_text(table, buf_idx, sc4_x + 12, row2_y + 12, "REMAINING", DML_COLOR_TEXT_MUTED, 0);
    char rem_str[32];
    snprintf(rem_str, sizeof(rem_str), "%u", (unsigned int)status->shots_remaining);
    dml_draw_buffer_draw_text_scaled(table, buf_idx, sc4_x + 12, row2_y + 36, rem_str, DML_COLOR_ACCENT_GREEN, 0, 2);
    dml_draw_buffer_draw_text(table, buf_idx, sc4_x + 12, row2_y + 76, "SD Card [OK]", DML_COLOR_TEXT_MUTED, 0);

    /* 6. Footer (y: 436..468) */
    int footer_y = 436;
    dml_draw_buffer_fill_rect(table, buf_idx, 16, footer_y, 688, 30, DML_COLOR_CARD_BG);
    dml_draw_buffer_draw_rect(table, buf_idx, 16, footer_y, 688, 30, DML_COLOR_CARD_BORDER);

    dml_draw_buffer_draw_text(table, buf_idx, 28, footer_y + 11, "D3300-ML v0.2.0 (Milbeaut-EXPEED4)", DML_COLOR_TEXT_MUTED, 0);
    dml_draw_buffer_draw_text(table, buf_idx, 410, footer_y + 11, "[INFO] Toggle  [MENU] ML OSD", DML_COLOR_TEXT_PRIMARY, 0);

    return 0;
}

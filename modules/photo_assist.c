#include "photo_assist.h"
#include <stdlib.h>
#include <string.h>

const char *dml_photo_assist_get_peaking_color_name(DmlPeakingColor col) {
    switch (col) {
        case DML_PEAK_OFF:    return "OFF";
        case DML_PEAK_GREEN:  return "GREEN";
        case DML_PEAK_RED:    return "RED";
        case DML_PEAK_CYAN:   return "CYAN";
        case DML_PEAK_YELLOW: return "YELLOW";
        case DML_PEAK_WHITE:  return "WHITE";
        default:              return "OFF";
    }
}

const char *dml_photo_assist_get_peaking_sens_name(DmlPeakingSens sens) {
    switch (sens) {
        case DML_PEAK_SENS_LOW:  return "LOW";
        case DML_PEAK_SENS_MED:  return "MEDIUM";
        case DML_PEAK_SENS_HIGH: return "HIGH";
        default:                 return "MEDIUM";
    }
}

const char *dml_photo_assist_get_zebra_level_name(DmlZebraLevel lvl) {
    switch (lvl) {
        case DML_ZEBRA_OFF: return "OFF";
        case DML_ZEBRA_100: return "100%";
        case DML_ZEBRA_95:  return "95%";
        case DML_ZEBRA_90:  return "90%";
        case DML_ZEBRA_75:  return "75%";
        default:            return "OFF";
    }
}

const char *dml_photo_assist_get_histogram_mode_name(DmlHistogramMode mode) {
    switch (mode) {
        case DML_HISTO_OFF:  return "OFF";
        case DML_HISTO_LUMA: return "LUMA";
        case DML_HISTO_RGB:  return "RGB";
        default:             return "OFF";
    }
}

uint32_t dml_photo_assist_get_peaking_rgb(DmlPeakingColor col) {
    switch (col) {
        case DML_PEAK_GREEN:  return 0x00FF66u;
        case DML_PEAK_RED:    return 0xFF2244u;
        case DML_PEAK_CYAN:   return 0x00EEFFu;
        case DML_PEAK_YELLOW: return 0xFFDD00u;
        case DML_PEAK_WHITE:  return 0xFFFFFFu;
        case DML_PEAK_OFF:
        default:              return 0x000000u;
    }
}

void dml_photo_assist_apply_peaking(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    DmlPeakingColor color,
    DmlPeakingSens sens
) {
    if (!table || buf_idx < 0 || !luma_frame || width < 3 || height < 3) {
        return;
    }
    if (color == DML_PEAK_OFF) {
        return;
    }

    uint32_t rgb_val = dml_photo_assist_get_peaking_rgb(color);
    int thresh;
    switch (sens) {
        case DML_PEAK_SENS_LOW:  thresh = 90; break;
        case DML_PEAK_SENS_HIGH: thresh = 32; break;
        case DML_PEAK_SENS_MED:
        default:                 thresh = 55; break;
    }

    /* Fast Sobel-like discrete gradient operator */
    for (int y = 1; y < height - 1; y++) {
        const uint8_t *row_prev = luma_frame + (y - 1) * width;
        const uint8_t *row_curr = luma_frame + y * width;
        const uint8_t *row_next = luma_frame + (y + 1) * width;

        for (int x = 1; x < width - 1; x++) {
            int gx = (int)row_curr[x + 1] - (int)row_curr[x - 1];
            int gy = (int)row_next[x] - (int)row_prev[x];
            int grad = abs(gx) + abs(gy);

            if (grad >= thresh) {
                dml_draw_buffer_put_pixel(table, buf_idx, x, y, rgb_val);
            }
        }
    }
}

void dml_photo_assist_apply_zebra(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    DmlZebraLevel level,
    uint32_t anim_tick
) {
    if (!table || buf_idx < 0 || !luma_frame || width <= 0 || height <= 0) {
        return;
    }
    if (level == DML_ZEBRA_OFF) {
        return;
    }

    uint8_t thresh;
    switch (level) {
        case DML_ZEBRA_100: thresh = 254; break;
        case DML_ZEBRA_95:  thresh = 242; break;
        case DML_ZEBRA_90:  thresh = 229; break;
        case DML_ZEBRA_75:  thresh = 191; break;
        default:            return;
    }

    int phase = (int)(anim_tick & 7u);
    for (int y = 0; y < height; y++) {
        const uint8_t *row = luma_frame + y * width;
        for (int x = 0; x < width; x++) {
            if (row[x] >= thresh) {
                int stripe = ((x + y + phase) & 7);
                if (stripe < 3) {
                    dml_draw_buffer_put_pixel(table, buf_idx, x, y, 0x111118u);
                } else {
                    dml_draw_buffer_put_pixel(table, buf_idx, x, y, 0xFF3333u);
                }
            }
        }
    }
}

void dml_photo_assist_draw_histogram(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    int hud_x,
    int hud_y,
    DmlHistogramMode mode
) {
    if (!table || buf_idx < 0 || !luma_frame || width <= 0 || height <= 0) {
        return;
    }
    if (mode == DML_HISTO_OFF) {
        return;
    }

    const int hud_w = 132;
    const int hud_h = 60;

    /* Draw HUD backdrop and border */
    dml_draw_buffer_fill_rect(table, buf_idx, hud_x, hud_y, hud_w, hud_h, 0x12121Cu);
    dml_draw_buffer_draw_rect(table, buf_idx, hud_x, hud_y, hud_w, hud_h, 0x3E3E55u);

    /* Vertical 25%, 50%, 75% reference grid lines */
    for (int q = 1; q <= 3; q++) {
        int gx = hud_x + 2 + q * 32;
        for (int gy = hud_y + 2; gy < hud_y + hud_h - 2; gy += 4) {
            dml_draw_buffer_put_pixel(table, buf_idx, gx, gy, 0x2A2A3Eu);
        }
    }

    /* Accumulate 64 bins (4 luma levels per bin) */
    uint32_t bins[64] = {0};
    uint32_t max_count = 1;
    uint32_t total_samples = 0;

    /* Subsample step 2 for sub-millisecond execution */
    for (int y = 0; y < height; y += 2) {
        const uint8_t *row = luma_frame + y * width;
        for (int x = 0; x < width; x += 2) {
            uint8_t bin = row[x] >> 2; /* 0..63 */
            bins[bin]++;
            total_samples++;
            if (bins[bin] > max_count) {
                max_count = bins[bin];
            }
        }
    }

    /* Draw 64 histogram columns */
    const int max_bar_h = hud_h - 6;
    uint32_t col_bar = (mode == DML_HISTO_RGB) ? 0x00E5FFu : 0x00FF88u;

    for (int b = 0; b < 64; b++) {
        int bar_h = (int)((bins[b] * (uint32_t)max_bar_h) / max_count);
        if (bar_h > max_bar_h) bar_h = max_bar_h;
        if (bar_h < 1 && bins[b] > 0) bar_h = 1;

        int bx = hud_x + 2 + b * 2;
        int by = hud_y + hud_h - 3 - bar_h;
        if (bar_h > 0) {
            dml_draw_buffer_fill_rect(table, buf_idx, bx, by, 2, bar_h, col_bar);
        }
    }

    /* Shadow / Highlight clipping warnings (red alert markers) */
    if (total_samples > 0) {
        uint32_t clip_thresh = total_samples / 50; /* >2% of pixels clipped */
        if (bins[0] > clip_thresh) {
            /* Shadow clipping alert dot at bottom-left */
            dml_draw_buffer_fill_rect(table, buf_idx, hud_x + 4, hud_y + 4, 4, 4, 0xFF2233u);
        }
        if (bins[63] > clip_thresh) {
            /* Highlight clipping alert dot at bottom-right */
            dml_draw_buffer_fill_rect(table, buf_idx, hud_x + hud_w - 8, hud_y + 4, 4, 4, 0xFF2233u);
        }
    }
}

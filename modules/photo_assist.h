#ifndef DML_PHOTO_ASSIST_H
#define DML_PHOTO_ASSIST_H

#include "../core/dml_draw_buffer.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    DML_PEAK_OFF    = 0,
    DML_PEAK_GREEN  = 1,
    DML_PEAK_RED    = 2,
    DML_PEAK_CYAN   = 3,
    DML_PEAK_YELLOW = 4,
    DML_PEAK_WHITE  = 5,
    DML_PEAK_COLOR_COUNT = 6
} DmlPeakingColor;

typedef enum {
    DML_PEAK_SENS_LOW  = 0,
    DML_PEAK_SENS_MED  = 1,
    DML_PEAK_SENS_HIGH = 2,
    DML_PEAK_SENS_COUNT = 3
} DmlPeakingSens;

typedef enum {
    DML_ZEBRA_OFF  = 0,
    DML_ZEBRA_100  = 1,
    DML_ZEBRA_95   = 2,
    DML_ZEBRA_90   = 3,
    DML_ZEBRA_75   = 4,
    DML_ZEBRA_COUNT = 5
} DmlZebraLevel;

typedef enum {
    DML_HISTO_OFF   = 0,
    DML_HISTO_LUMA  = 1,
    DML_HISTO_RGB   = 2,
    DML_HISTO_COUNT = 3
} DmlHistogramMode;

/* Display names for UI */
const char *dml_photo_assist_get_peaking_color_name(DmlPeakingColor col);
const char *dml_photo_assist_get_peaking_sens_name(DmlPeakingSens sens);
const char *dml_photo_assist_get_zebra_level_name(DmlZebraLevel lvl);
const char *dml_photo_assist_get_histogram_mode_name(DmlHistogramMode mode);

/* RGB hex mapping for overlay rendering */
uint32_t dml_photo_assist_get_peaking_rgb(DmlPeakingColor col);

/* Image analysis and overlay rendering */
void dml_photo_assist_apply_peaking(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    DmlPeakingColor color,
    DmlPeakingSens sens
);

void dml_photo_assist_apply_zebra(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    DmlZebraLevel level,
    uint32_t anim_tick
);

void dml_photo_assist_draw_histogram(
    DmlDrawBufferTable *table,
    int buf_idx,
    const uint8_t *luma_frame,
    int width,
    int height,
    int hud_x,
    int hud_y,
    DmlHistogramMode mode
);

#endif /* DML_PHOTO_ASSIST_H */

#include "dml_camera_status_hw.h"
#include <string.h>

/* Shutter speed lookup table (Nikon 1/3 EV steps, 0..52)
   code 0: BULB
   code 1..16: 30" down to 1"
   code 17..52: 1/1.3 down to 1/4000 */
typedef struct {
    uint32_t num;
    uint32_t den;
    uint8_t  is_bulb;
} DmlShutterMapping;

static const DmlShutterMapping kShutterTable[] = {
    {0, 0, 1},       /* 0: BULB */
    {30, 1, 0},      /* 1: 30" */
    {25, 1, 0},      /* 2: 25" */
    {20, 1, 0},      /* 3: 20" */
    {15, 1, 0},      /* 4: 15" */
    {13, 1, 0},      /* 5: 13" */
    {10, 1, 0},      /* 6: 10" */
    {8, 1, 0},       /* 7: 8" */
    {6, 1, 0},       /* 8: 6" */
    {5, 1, 0},       /* 9: 5" */
    {4, 1, 0},       /* 10: 4" */
    {3, 1, 0},       /* 11: 3" */
    {25, 10, 0},     /* 12: 2.5" */
    {2, 1, 0},       /* 13: 2" */
    {16, 10, 0},     /* 14: 1.6" */
    {13, 10, 0},     /* 15: 1.3" */
    {1, 1, 0},       /* 16: 1" */
    {10, 13, 0},     /* 17: 1/1.3 */
    {10, 16, 0},     /* 18: 1/1.6 */
    {1, 2, 0},       /* 19: 1/2 */
    {10, 25, 0},     /* 20: 1/2.5 */
    {1, 3, 0},       /* 21: 1/3 */
    {1, 4, 0},       /* 22: 1/4 */
    {1, 5, 0},       /* 23: 1/5 */
    {1, 6, 0},       /* 24: 1/6 */
    {1, 8, 0},       /* 25: 1/8 */
    {1, 10, 0},      /* 26: 1/10 */
    {1, 13, 0},      /* 27: 1/13 */
    {1, 15, 0},      /* 28: 1/15 */
    {1, 20, 0},      /* 29: 1/20 */
    {1, 25, 0},      /* 30: 1/25 */
    {1, 30, 0},      /* 31: 1/30 */
    {1, 40, 0},      /* 32: 1/40 */
    {1, 50, 0},      /* 33: 1/50 */
    {1, 60, 0},      /* 34: 1/60 */
    {1, 80, 0},      /* 35: 1/80 */
    {1, 100, 0},     /* 36: 1/100 */
    {1, 125, 0},     /* 37: 1/125 */
    {1, 160, 0},     /* 38: 1/160 */
    {1, 200, 0},     /* 39: 1/200 */
    {1, 250, 0},     /* 40: 1/250 */
    {1, 320, 0},     /* 41: 1/320 */
    {1, 400, 0},     /* 42: 1/400 */
    {1, 500, 0},     /* 43: 1/500 */
    {1, 640, 0},     /* 44: 1/640 */
    {1, 800, 0},     /* 45: 1/800 */
    {1, 1000, 0},    /* 46: 1/1000 */
    {1, 1250, 0},    /* 47: 1/1250 */
    {1, 1600, 0},    /* 48: 1/1600 */
    {1, 2000, 0},    /* 49: 1/2000 */
    {1, 2500, 0},    /* 50: 1/2500 */
    {1, 3200, 0},    /* 51: 1/3200 */
    {1, 4000, 0}     /* 52: 1/4000 */
};
#define K_SHUTTER_TABLE_COUNT (sizeof(kShutterTable) / sizeof(kShutterTable[0]))

/* Aperture lookup table in centi-f-stops (1/3 EV steps, 0..30) */
static const uint16_t kApertureTable[] = {
    100, /* 0: f/1.0 */
    110, /* 1: f/1.1 */
    120, /* 2: f/1.2 */
    140, /* 3: f/1.4 */
    160, /* 4: f/1.6 */
    180, /* 5: f/1.8 */
    200, /* 6: f/2.0 */
    220, /* 7: f/2.2 */
    250, /* 8: f/2.5 */
    280, /* 9: f/2.8 */
    320, /* 10: f/3.2 */
    350, /* 11: f/3.5 */
    400, /* 12: f/4.0 */
    450, /* 13: f/4.5 */
    500, /* 14: f/5.0 */
    560, /* 15: f/5.6 */
    630, /* 16: f/6.3 */
    710, /* 17: f/7.1 */
    800, /* 18: f/8.0 */
    900, /* 19: f/9.0 */
    1000, /* 20: f/10 */
    1100, /* 21: f/11 */
    1300, /* 22: f/13 */
    1400, /* 23: f/14 */
    1600, /* 24: f/16 */
    1800, /* 25: f/18 */
    2000, /* 26: f/20 */
    2200, /* 27: f/22 */
    2500, /* 28: f/25 */
    2900, /* 29: f/29 */
    3200  /* 30: f/32 */
};
#define K_APERTURE_TABLE_COUNT (sizeof(kApertureTable) / sizeof(kApertureTable[0]))

/* ISO lookup table */
static const uint32_t kIsoTable[] = {
    100,
    200,
    400,
    800,
    1600,
    3200,
    6400,
    12800,
    25600
};
#define K_ISO_TABLE_COUNT (sizeof(kIsoTable) / sizeof(kIsoTable[0]))

void dml_hw_map_shutter(uint8_t tv_code, uint32_t *num, uint32_t *den, uint8_t *is_bulb) {
    if (!num || !den || !is_bulb) return;
    if (tv_code >= K_SHUTTER_TABLE_COUNT) {
        /* Default fallback to 1/250s */
        *num = 1;
        *den = 250;
        *is_bulb = 0;
        return;
    }
    *num = kShutterTable[tv_code].num;
    *den = kShutterTable[tv_code].den;
    *is_bulb = kShutterTable[tv_code].is_bulb;
}

uint16_t dml_hw_map_aperture(uint8_t av_code) {
    if (av_code >= K_APERTURE_TABLE_COUNT) {
        return 280; /* Default fallback f/2.8 */
    }
    return kApertureTable[av_code];
}

uint32_t dml_hw_map_iso(uint8_t iso_code, uint8_t *is_auto) {
    uint8_t auto_flag = (iso_code & 0x80u) ? 1 : 0;
    uint8_t index = iso_code & 0x7Fu;
    if (is_auto) {
        *is_auto = auto_flag;
    }
    if (index >= K_ISO_TABLE_COUNT) {
        return 400; /* Default fallback ISO 400 */
    }
    return kIsoTable[index];
}

int dml_camera_status_read_hw(DmlCameraStatus *out_status, const void *raw_ptr) {
    if (!out_status) {
        return -1;
    }

    const DmlHwCameraStatusRaw *raw = (const DmlHwCameraStatusRaw *)raw_ptr;
    if (!raw) {
#if defined(__arm__)
        raw = (const DmlHwCameraStatusRaw *)D3300_ADDR_CKRRC_CAMERA_STATUS;
#else
        return -1; /* Mock pointer required on non-ARM host environments */
#endif
    }

    /* Basic sanity check: vtable must not be completely zeroed */
    if (raw->vtable == 0) {
        return -1;
    }

    memset(out_status, 0, sizeof(*out_status));

    /* 1. Shooting Mode */
    uint32_t mode = raw->mode_flags & 0x07u;
    if (mode > DML_SHOOTING_MODE_EFFECTS) {
        mode = DML_SHOOTING_MODE_MANUAL;
    }
    out_status->shooting_mode = (DmlShootingMode)mode;

    /* 2. Shutter Speed */
    dml_hw_map_shutter(raw->tv_code, &out_status->shutter_numerator,
                       &out_status->shutter_denominator, &out_status->is_bulb);

    /* 3. Aperture */
    out_status->aperture_centi = dml_hw_map_aperture(raw->av_code);

    /* 4. ISO */
    out_status->iso = dml_hw_map_iso(raw->iso_code, &out_status->is_auto_iso);

    /* 5. EV Compensation (-9..+9 in 1/3 EV) */
    int8_t ev = raw->ev_compensation;
    if (ev < -9) ev = -9;
    if (ev > 9)  ev = 9;
    out_status->ev_compensation_third_stops = ev;

    /* 6. Metering Mode */
    uint8_t metering = raw->metering_mode;
    if (metering > DML_METERING_SPOT) metering = DML_METERING_MATRIX;
    out_status->metering_mode = (DmlMeteringMode)metering;

    /* 7. AF Mode & Point */
    uint8_t af = raw->af_mode;
    if (af > DML_AF_MODE_MF) af = DML_AF_MODE_AF_S;
    out_status->af_mode = (DmlAFMode)af;

    uint8_t point = raw->active_focus_point;
    if (point > 10) point = 0; /* Clamp to 11-point array: 0..10 */
    out_status->active_focus_point = point;

    /* 8. Release Mode */
    uint8_t rel = raw->release_mode;
    if (rel > DML_RELEASE_TIMER) rel = DML_RELEASE_SINGLE;
    out_status->release_mode = (DmlReleaseMode)rel;

    /* 9. Flash Configuration */
    out_status->flash_raised = (raw->flash_state & 0x80u) ? 1 : 0;
    uint8_t fmode = raw->flash_state & 0x0Fu;
    if (fmode > DML_FLASH_MODE_OFF) fmode = DML_FLASH_MODE_AUTO;
    out_status->flash_mode = (DmlFlashMode)fmode;

    int8_t f_ev = raw->flash_compensation;
    if (f_ev < -9) f_ev = -9;
    if (f_ev > 9)  f_ev = 9;
    out_status->flash_compensation_third_stops = f_ev;

    /* 10. Battery */
    uint8_t b_pct = raw->battery_status & 0x7Fu;
    if (b_pct > 100) b_pct = 100;
    out_status->battery_pct = b_pct;
    out_status->battery_warning = (raw->battery_status & 0x80u) ? 1 : (b_pct < 15 ? 1 : 0);

    /* 11. Image Quality */
    uint8_t qual = raw->image_quality;
    if (qual > DML_QUALITY_BASIC) qual = DML_QUALITY_RAW_FINE;
    out_status->image_quality = (DmlImageQuality)qual;

    /* 12. Storage & Buffer */
    out_status->shots_remaining = raw->shots_remaining;
    out_status->buffer_remaining = raw->buffer_remaining;

    /* 13. White Balance */
    uint8_t wb = raw->white_balance;
    if (wb > DML_WB_PRESET_MANUAL) wb = DML_WB_AUTO;
    out_status->white_balance = (DmlWhiteBalance)wb;

    return 0;
}

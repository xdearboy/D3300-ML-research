#ifndef DML_CAMERA_STATUS_H
#define DML_CAMERA_STATUS_H

#include <stddef.h>
#include <stdint.h>

/* Reverse-engineered D3300 camera status and shooting parameter state.
   Mapped from:
   - 17CKRRCCameraStatus at RAM 0x10491580 (vtable 0x9151F6F0, method +0x14)
   - 27CINFOModifyDataShootingMenu at RAM 0x915073A4
   - CSYSCPowerManager at RAM 0x912838A0 (TYSensReadPort) */

typedef enum {
    DML_SHOOTING_MODE_MANUAL = 0,             /* M */
    DML_SHOOTING_MODE_APERTURE_PRIORITY = 1,   /* A */
    DML_SHOOTING_MODE_SHUTTER_PRIORITY = 2,    /* S */
    DML_SHOOTING_MODE_PROGRAM = 3,             /* P */
    DML_SHOOTING_MODE_AUTO = 4,                /* AUTO */
    DML_SHOOTING_MODE_GUIDE = 5,               /* GUIDE */
    DML_SHOOTING_MODE_EFFECTS = 6              /* EFFECTS */
} DmlShootingMode;

typedef enum {
    DML_AF_MODE_AF_S = 0,                      /* Single-servo AF */
    DML_AF_MODE_AF_C = 1,                      /* Continuous-servo AF */
    DML_AF_MODE_AF_A = 2,                      /* Auto-servo AF */
    DML_AF_MODE_MF = 3                         /* Manual focus */
} DmlAFMode;

typedef enum {
    DML_METERING_MATRIX = 0,                   /* Matrix 3D Color */
    DML_METERING_CENTER_WEIGHTED = 1,          /* Center-weighted */
    DML_METERING_SPOT = 2                      /* Spot */
} DmlMeteringMode;

typedef enum {
    DML_RELEASE_SINGLE = 0,                    /* Single frame [S] */
    DML_RELEASE_CONTINUOUS = 1,                /* Continuous 5 fps [C] */
    DML_RELEASE_QUIET = 2,                     /* Quiet shutter [Q] */
    DML_RELEASE_TIMER = 3                      /* Self-timer 10s */
} DmlReleaseMode;

typedef enum {
    DML_QUALITY_RAW_FINE = 0,                  /* NEF (RAW) + JPEG Fine */
    DML_QUALITY_RAW = 1,                       /* NEF (RAW) */
    DML_QUALITY_FINE = 2,                      /* JPEG Fine */
    DML_QUALITY_NORM = 3,                      /* JPEG Normal */
    DML_QUALITY_BASIC = 4                      /* JPEG Basic */
} DmlImageQuality;

typedef enum {
    DML_WB_AUTO = 0,                           /* Auto White Balance */
    DML_WB_INCANDESCENT = 1,                   /* Incandescent */
    DML_WB_FLUORESCENT = 2,                    /* Fluorescent */
    DML_WB_DIRECT_SUNLIGHT = 3,                /* Direct sunlight */
    DML_WB_FLASH = 4,                          /* Flash */
    DML_WB_CLOUDY = 5,                         /* Cloudy */
    DML_WB_SHADE = 6,                          /* Shade */
    DML_WB_PRESET_MANUAL = 7                   /* Preset manual */
} DmlWhiteBalance;

/* Complete camera exposure and operational snapshot */
typedef struct {
    /* Primary exposure parameters */
    DmlShootingMode shooting_mode;
    uint32_t shutter_numerator;                /* e.g., 1 */
    uint32_t shutter_denominator;              /* e.g., 250 (1/250s), 1 (1s), 0 for BULB */
    uint8_t is_bulb;                           /* 1 if bulb exposure */

    uint16_t aperture_centi;                   /* centi-f-stops: 280 = f/2.8, 560 = f/5.6 */

    uint32_t iso;                              /* 100..25600 */
    uint8_t is_auto_iso;                       /* 1 if auto-ISO enabled */

    int8_t ev_compensation_third_stops;        /* 1/3 EV steps: 0 = 0.0, +3 = +1.0, -9 = -3.0 */

    /* Power and storage */
    uint8_t battery_pct;                       /* 0..100% */
    uint8_t battery_warning;                   /* 0: OK, 1: Low, 2: Critical */
    uint32_t shots_remaining;                  /* e.g., 1420 */

    /* Focus and capture configuration */
    DmlAFMode af_mode;
    DmlMeteringMode metering_mode;
    DmlReleaseMode release_mode;
    DmlImageQuality image_quality;
    DmlWhiteBalance white_balance;

    /* Status indicators */
    uint8_t exposure_locked;                   /* AE-L */
    uint8_t focus_locked;                      /* AF-L */
    uint8_t flash_ready;                       /* Flash strobe ready */
    uint8_t active_d_lighting;                 /* Active D-Lighting enabled */
} DmlCameraStatus;

/* Initializes status to typical default D3300 Manual exposure:
   Manual mode, 1/250s, f/2.8, ISO 400, 0.0 EV, 84% battery, 1420 shots, AF-S, Matrix */
void dml_camera_status_init_default(DmlCameraStatus *status);

/* Formats shutter speed to human-readable string:
   - "BULB"
   - "1/250", "1/4000"
   - "2\"", "30\""
   - "1.3\"" */
int dml_camera_status_format_shutter(const DmlCameraStatus *status, char *out_buf, size_t max_len);

/* Formats aperture to human-readable string:
   - "F2.8", "F1.4", "F5.6", "F22" */
int dml_camera_status_format_aperture(const DmlCameraStatus *status, char *out_buf, size_t max_len);

/* Formats ISO to human-readable string:
   - "100", "400", "6400", "Hi 1" (12800), "Hi 2" (25600), "A400" (Auto ISO) */
int dml_camera_status_format_iso(const DmlCameraStatus *status, char *out_buf, size_t max_len);

/* Formats EV compensation to human-readable string:
   - " 0.0", "+1.0", "-0.3", "+2.7" */
int dml_camera_status_format_ev(const DmlCameraStatus *status, char *out_buf, size_t max_len);

/* String lookups for enums */
const char *dml_camera_status_mode_name(DmlShootingMode mode);
const char *dml_camera_status_af_name(DmlAFMode af);
const char *dml_camera_status_metering_name(DmlMeteringMode metering);
const char *dml_camera_status_release_name(DmlReleaseMode release);
const char *dml_camera_status_quality_name(DmlImageQuality quality);
const char *dml_camera_status_wb_name(DmlWhiteBalance wb);

#endif /* DML_CAMERA_STATUS_H */

#ifndef DML_FUJI_FILM_SIM_H
#define DML_FUJI_FILM_SIM_H

#include <stddef.h>
#include <stdint.h>

#define DML_NCP_FILE_SIZE 700u

typedef enum {
    DML_FUJI_CLASSIC_CHROME = 0,
    DML_FUJI_VELVIA         = 1,
    DML_FUJI_ACROS          = 2,
    DML_FUJI_CLASSIC_NEG    = 3,
    DML_FUJI_ASTIA          = 4,
    DML_FUJI_COUNT          = 5
} DmlFujiProfileId;

typedef struct {
    const char *name;            /* 20-char max profile name */
    const char *desc;            /* Short UI description */
    uint16_t   base_profile_id; /* Nikon base Picture Control code */
    int8_t     sharpening;      /* -3..+3 */
    int8_t     contrast;        /* -127 for custom curve */
    int8_t     brightness;      /* -127 for custom curve */
    int8_t     saturation;      /* -3..+3 */
    int8_t     hue;             /* -3..+3 */
    uint8_t    mono_filter;     /* 0: OFF, 1: YELLOW, 4: GREEN */
    uint8_t    mono_toning;     /* 0: B/W */
    uint16_t   lut[256];        /* 256-point 16-bit curve (0..65535) */
} DmlFujiProfile;

/* Get profile definition by ID */
const DmlFujiProfile *dml_fuji_get_profile(DmlFujiProfileId id);

/* Serialize 700-byte valid Nikon .NCP / .NOP binary */
int dml_fuji_generate_ncp(const DmlFujiProfile *profile, uint8_t *out_buf, size_t max_len, size_t *out_size);

/* Write profile directly to SD card path (<sd_root>/NIKON/CUSTOMPC/PICCON%02d.NOP and .NCP) */
int dml_fuji_export_to_sd(const DmlFujiProfile *profile, const char *sd_root, int slot_index);

/* Export all 5 profiles into specified directory */
int dml_fuji_export_all(const char *dest_dir);

#endif /* DML_FUJI_FILM_SIM_H */

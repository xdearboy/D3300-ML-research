#include "fuji_film_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

/* Verified Nikon Picture Control Base Profile Codes */
#define NIKON_BASE_STANDARD   0x0001u
#define NIKON_BASE_NEUTRAL    0x03C2u
#define NIKON_BASE_VIVID      0x00C3u
#define NIKON_BASE_PORTRAIT   0x0486u
#define NIKON_BASE_LANDSCAPE  0x04C7u
#define NIKON_BASE_MONOCHROME 0x064Du

static DmlFujiProfile g_fuji_profiles[DML_FUJI_COUNT];
static int g_fuji_initialized = 0;

static uint16_t clamp_u16(double val) {
    if (val < 0.0) return 0;
    if (val > 65535.0) return 65535;
    return (uint16_t)(val + 0.5);
}

/* Helper to build a smooth, natural tone response curve */
static void build_tone_curve(
    uint16_t *lut,
    double black_lift,
    double shadow_slope,
    double mid_contrast,
    double hl_compress
) {
    for (int i = 0; i < 256; i++) {
        double x = (double)i / 255.0; /* 0.0 .. 1.0 */
        double y;

        /* S-curve sigmoid formulation with shadow lift & highlight roll-off */
        if (x < 0.5) {
            double norm_x = x * 2.0;
            double p = shadow_slope * mid_contrast;
            double curve = pow(norm_x, p);
            y = black_lift + (0.5 - black_lift) * curve;
        } else {
            double norm_x = (x - 0.5) * 2.0;
            double p = hl_compress * mid_contrast;
            double high_val = 1.0 - pow(1.0 - norm_x, p);
            y = 0.5 + 0.5 * high_val;
        }

        if (y < 0.0) y = 0.0;
        if (y > 1.0) y = 1.0;
        lut[i] = clamp_u16(y * 65535.0);
    }
}

static void ensure_fuji_initialized(void) {
    if (g_fuji_initialized) return;

    /* 1. CLASSIC CHROME: Muted saturation, warm amber/cyan shifts, lifted matte shadows, soft highlights */
    g_fuji_profiles[DML_FUJI_CLASSIC_CHROME] = (DmlFujiProfile){
        .name = "CLASSIC CHROME",
        .desc = "Muted saturation & deep shadows",
        .base_profile_id = NIKON_BASE_NEUTRAL,
        .sharpening = 2,
        .contrast = -127,   /* Custom curve */
        .brightness = -127, /* Custom curve */
        .saturation = -1,   /* Muted tones */
        .hue = 0,
        .mono_filter = 0,
        .mono_toning = 0
    };
    build_tone_curve(g_fuji_profiles[DML_FUJI_CLASSIC_CHROME].lut, 0.022, 1.15, 1.10, 1.18);

    /* 2. FUJI VELVIA: Punchy high saturation, deep rich blacks, vivid greens and blues */
    g_fuji_profiles[DML_FUJI_VELVIA] = (DmlFujiProfile){
        .name = "FUJI VELVIA",
        .desc = "Vivid landscape & deep contrast",
        .base_profile_id = NIKON_BASE_VIVID,
        .sharpening = 3,
        .contrast = -127,
        .brightness = -127,
        .saturation = 2,    /* High saturation */
        .hue = 0,
        .mono_filter = 0,
        .mono_toning = 0
    };
    build_tone_curve(g_fuji_profiles[DML_FUJI_VELVIA].lut, 0.000, 1.30, 1.25, 0.90);

    /* 3. FUJI ACROS: High micro-contrast black-and-white with rich shadows */
    g_fuji_profiles[DML_FUJI_ACROS] = (DmlFujiProfile){
        .name = "FUJI ACROS",
        .desc = "Rich B/W with yellow filter response",
        .base_profile_id = NIKON_BASE_MONOCHROME,
        .sharpening = 3,
        .contrast = -127,
        .brightness = -127,
        .saturation = 0,
        .hue = 0,
        .mono_filter = 1,   /* Yellow filter simulation */
        .mono_toning = 0
    };
    build_tone_curve(g_fuji_profiles[DML_FUJI_ACROS].lut, 0.005, 1.22, 1.18, 1.05);

    /* 4. CLASSIC NEGATIVE: Hard shadows, crushed near-blacks, high tonal grit */
    g_fuji_profiles[DML_FUJI_CLASSIC_NEG] = (DmlFujiProfile){
        .name = "CLASSIC NEGATIVE",
        .desc = "Superia 400 hard contrast & green cast",
        .base_profile_id = NIKON_BASE_STANDARD,
        .sharpening = 2,
        .contrast = -127,
        .brightness = -127,
        .saturation = 0,
        .hue = 1,           /* Characteristic green-cyan tint */
        .mono_filter = 0,
        .mono_toning = 0
    };
    /* Hard crushed shadows: first 8 values are 0 */
    build_tone_curve(g_fuji_profiles[DML_FUJI_CLASSIC_NEG].lut, 0.000, 1.40, 1.30, 1.15);
    for (int k = 0; k < 8; k++) {
        g_fuji_profiles[DML_FUJI_CLASSIC_NEG].lut[k] = 0;
    }

    /* 5. ASTIA / SOFT: Creamy skin tones, soft pastel gradations */
    g_fuji_profiles[DML_FUJI_ASTIA] = (DmlFujiProfile){
        .name = "FUJI ASTIA",
        .desc = "Soft pastel tones & natural skin",
        .base_profile_id = NIKON_BASE_PORTRAIT,
        .sharpening = 1,
        .contrast = -127,
        .brightness = -127,
        .saturation = 0,
        .hue = 0,
        .mono_filter = 0,
        .mono_toning = 0
    };
    build_tone_curve(g_fuji_profiles[DML_FUJI_ASTIA].lut, 0.015, 0.95, 0.92, 1.25);

    g_fuji_initialized = 1;
}

const DmlFujiProfile *dml_fuji_get_profile(DmlFujiProfileId id) {
    ensure_fuji_initialized();
    if ((int)id < 0 || (int)id >= (int)DML_FUJI_COUNT) {
        return NULL;
    }
    return &g_fuji_profiles[id];
}

int dml_fuji_generate_ncp(const DmlFujiProfile *profile, uint8_t *out_buf, size_t max_len, size_t *out_size) {
    if (!profile || !out_buf || max_len < DML_NCP_FILE_SIZE) {
        return -1;
    }

    ensure_fuji_initialized();
    memset(out_buf, 0, DML_NCP_FILE_SIZE);

    /* 1. Header (4 bytes): "NCP\0" */
    memcpy(out_buf, "NCP\0", 4);

    /* 2. Record 1: Profile metadata & baseline settings */
    /* Record ID = 1 (Big-Endian uint32) */
    out_buf[4] = 0; out_buf[5] = 0; out_buf[6] = 0; out_buf[7] = 1;
    /* Record Size = 36 (Big-Endian uint32) */
    out_buf[8] = 0; out_buf[9] = 0; out_buf[10] = 0; out_buf[11] = 36;

    /* Version = "0100" */
    memcpy(&out_buf[12], "0100", 4);

    /* Profile Name: 20 bytes null-padded */
    if (profile->name) {
        strncpy((char *)&out_buf[16], profile->name, 19);
    }

    /* Base Profile ID (Big-Endian uint16) */
    out_buf[36] = (uint8_t)((profile->base_profile_id >> 8) & 0xFFu);
    out_buf[37] = (uint8_t)(profile->base_profile_id & 0xFFu);

    /* Modified flag = 2, Pad = 0 */
    out_buf[38] = 2;
    out_buf[39] = 0;

    /* Sharpening (offset 40) */
    out_buf[40] = (uint8_t)(0x80 + profile->sharpening);

    /* Contrast (offset 41): 0x01 means custom curve */
    out_buf[41] = (profile->contrast == -127) ? 0x01u : (uint8_t)(0x80 + profile->contrast);

    /* Brightness (offset 42): 0x01 means custom curve */
    out_buf[42] = (profile->brightness == -127) ? 0x01u : (uint8_t)(0x80 + profile->brightness);

    /* Saturation (offset 43) */
    out_buf[43] = (uint8_t)(0x80 + profile->saturation);

    /* Hue (offset 44) */
    out_buf[44] = (uint8_t)(0x80 + profile->hue);

    /* Monochrome Filter (offset 45) */
    out_buf[45] = (uint8_t)(0x80 + profile->mono_filter);

    /* Monochrome Toning (offset 46) */
    out_buf[46] = (uint8_t)(0x80 + profile->mono_toning);

    /* Toning Strength (offset 47) */
    out_buf[47] = 0x80u;

    /* 3. Record 2: Tone Curve & 256-point 16-bit LUT */
    /* Record ID = 2 (Big-Endian uint32) */
    out_buf[48] = 0; out_buf[49] = 0; out_buf[50] = 0; out_buf[51] = 2;
    /* Record Size = 640 (0x0280 Big-Endian uint32) */
    out_buf[52] = 0; out_buf[53] = 0; out_buf[54] = 0x02; out_buf[55] = 0x80;

    /* Offset 56..57: Pad (2 bytes: 0, 0) */
    out_buf[56] = 0; out_buf[57] = 0;

    /* Offset 58..59: Black point (0), White point (255) */
    out_buf[58] = 0;
    out_buf[59] = 255;

    /* Offset 60..61: Pad (2 bytes: 0, 0) */
    out_buf[60] = 0; out_buf[61] = 0;

    /* Offset 62..63: Halftone point (128, 0) */
    out_buf[62] = 128;
    out_buf[63] = 0;

    /* Offset 64..65: Out min (0), Out max (255) */
    out_buf[64] = (uint8_t)(profile->lut[0] / 256u);
    out_buf[65] = (uint8_t)(profile->lut[255] / 256u);

    /* Offset 66..67: Pad (2 bytes: 0, 0) */
    out_buf[66] = 0; out_buf[67] = 0;

    /* Offset 68: Number of polynomial points = 4 */
    out_buf[68] = 4;

    /* Offset 69..76: 4 key coordinate pairs (in, out) */
    out_buf[69] = 0;   out_buf[70] = (uint8_t)(profile->lut[0] / 256u);
    out_buf[71] = 64;  out_buf[72] = (uint8_t)(profile->lut[64] / 256u);
    out_buf[73] = 192; out_buf[74] = (uint8_t)(profile->lut[192] / 256u);
    out_buf[75] = 255; out_buf[76] = (uint8_t)(profile->lut[255] / 256u);

    /* Offset 77..125: Remaining point buffer zeroed */
    /* Offset 126..183: 58 bytes padding zeroed */

    /* Offset 184..695: 256 * 2 bytes = 512 bytes Big-Endian 16-bit LUT */
    for (int i = 0; i < 256; i++) {
        uint16_t val = profile->lut[i];
        out_buf[184 + i * 2]     = (uint8_t)((val >> 8) & 0xFFu);
        out_buf[184 + i * 2 + 1] = (uint8_t)(val & 0xFFu);
    }

    /* 4. Record 0: End of File (4 bytes of zero) */
    out_buf[696] = 0; out_buf[697] = 0; out_buf[698] = 0; out_buf[699] = 0;

    if (out_size) {
        *out_size = DML_NCP_FILE_SIZE;
    }
    return 0;
}

static void create_dirs_recursive(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    if (!path || !*path) return;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
#if defined(_WIN32)
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#if defined(_WIN32)
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

int dml_fuji_export_to_sd(const DmlFujiProfile *profile, const char *sd_root, int slot_index) {
    if (!profile || !sd_root || slot_index < 1 || slot_index > 99) {
        return -1;
    }

    uint8_t buf[DML_NCP_FILE_SIZE];
    size_t size = 0;
    if (dml_fuji_generate_ncp(profile, buf, sizeof(buf), &size) != 0) {
        return -1;
    }

    char dir_custompc[512];
    char file_nop[512];
    char file_ncp[512];

    int dir_len = snprintf(dir_custompc, sizeof(dir_custompc), "%s/NIKON/CUSTOMPC", sd_root);
    if (dir_len < 0 || (size_t)dir_len >= sizeof(dir_custompc)) {
        return -1;
    }
    create_dirs_recursive(dir_custompc);

    /* Host-side prototype only: camera compatibility has not been established. */
    int nop_len = snprintf(file_nop, sizeof(file_nop), "%s/PICCON%02d.NOP", dir_custompc, slot_index);
    int ncp_len = snprintf(file_ncp, sizeof(file_ncp), "%s/PICCON%02d.NCP", dir_custompc, slot_index);
    if (nop_len < 0 || (size_t)nop_len >= sizeof(file_nop) ||
        ncp_len < 0 || (size_t)ncp_len >= sizeof(file_ncp)) {
        return -1;
    }

    FILE *fnop = fopen(file_nop, "wb");
    if (fnop) {
        fwrite(buf, 1, size, fnop);
        fclose(fnop);
    }

    FILE *fncp = fopen(file_ncp, "wb");
    if (!fncp) {
        return -1;
    }
    size_t written = fwrite(buf, 1, size, fncp);
    fclose(fncp);

    return (written == size) ? 0 : -1;
}

int dml_fuji_export_all(const char *dest_dir) {
    if (!dest_dir) return -1;
    ensure_fuji_initialized();

    create_dirs_recursive(dest_dir);

    for (int i = 0; i < (int)DML_FUJI_COUNT; i++) {
        const DmlFujiProfile *p = &g_fuji_profiles[i];
        if (dml_fuji_export_to_sd(p, dest_dir, i + 1) != 0) {
            return -1;
        }
    }
    return 0;
}

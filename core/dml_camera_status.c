#include "dml_camera_status.h"
#include <stdio.h>
#include <string.h>

void dml_camera_status_init_default(DmlCameraStatus *status) {
    if (!status) return;
    memset(status, 0, sizeof(*status));

    status->shooting_mode = DML_SHOOTING_MODE_MANUAL;
    status->shutter_numerator = 1;
    status->shutter_denominator = 250;
    status->is_bulb = 0;

    status->aperture_centi = 280; /* f/2.8 */
    status->iso = 400;
    status->is_auto_iso = 0;

    status->ev_compensation_third_stops = 0; /* 0.0 EV */
    status->battery_pct = 84;
    status->battery_warning = 0;
    status->shots_remaining = 1420;

    status->af_mode = DML_AF_MODE_AF_S;
    status->metering_mode = DML_METERING_MATRIX;
    status->release_mode = DML_RELEASE_SINGLE;
    status->image_quality = DML_QUALITY_RAW_FINE;
    status->white_balance = DML_WB_AUTO;

    status->exposure_locked = 0;
    status->focus_locked = 0;
    status->flash_ready = 1;
    status->active_d_lighting = 1;
}

int dml_camera_status_format_shutter(const DmlCameraStatus *status, char *out_buf, size_t max_len) {
    if (!status || !out_buf || max_len == 0) return -1;

    if (status->is_bulb) {
        return snprintf(out_buf, max_len, "BULB") > 0 ? 0 : -1;
    }

    if (status->shutter_denominator == 0) {
        return snprintf(out_buf, max_len, "--") > 0 ? 0 : -1;
    }

    if (status->shutter_denominator == 1) {
        return snprintf(out_buf, max_len, "%u\"", (unsigned int)status->shutter_numerator) > 0 ? 0 : -1;
    }

    if (status->shutter_numerator == 1) {
        return snprintf(out_buf, max_len, "1/%u", (unsigned int)status->shutter_denominator) > 0 ? 0 : -1;
    }

    /* Fractional seconds with decimal, e.g. 1.3" or 2.5" */
    double sec = (double)status->shutter_numerator / (double)status->shutter_denominator;
    return snprintf(out_buf, max_len, "%.1f\"", sec) > 0 ? 0 : -1;
}

int dml_camera_status_format_aperture(const DmlCameraStatus *status, char *out_buf, size_t max_len) {
    if (!status || !out_buf || max_len == 0) return -1;

    if (status->aperture_centi == 0) {
        return snprintf(out_buf, max_len, "F--") > 0 ? 0 : -1;
    }

    uint16_t whole = status->aperture_centi / 100u;
    uint16_t frac = (status->aperture_centi % 100u) / 10u;

    if (frac == 0) {
        return snprintf(out_buf, max_len, "F%u", (unsigned int)whole) > 0 ? 0 : -1;
    }
    return snprintf(out_buf, max_len, "F%u.%u", (unsigned int)whole, (unsigned int)frac) > 0 ? 0 : -1;
}

int dml_camera_status_format_iso(const DmlCameraStatus *status, char *out_buf, size_t max_len) {
    if (!status || !out_buf || max_len == 0) return -1;

    if (status->iso == 12800) {
        return snprintf(out_buf, max_len, "%sHi 1", status->is_auto_iso ? "A" : "") > 0 ? 0 : -1;
    }
    if (status->iso == 25600) {
        return snprintf(out_buf, max_len, "%sHi 2", status->is_auto_iso ? "A" : "") > 0 ? 0 : -1;
    }

    return snprintf(out_buf, max_len, "%s%u", status->is_auto_iso ? "A" : "", (unsigned int)status->iso) > 0 ? 0 : -1;
}

int dml_camera_status_format_ev(const DmlCameraStatus *status, char *out_buf, size_t max_len) {
    if (!status || !out_buf || max_len == 0) return -1;

    int8_t steps = status->ev_compensation_third_stops;
    if (steps == 0) {
        return snprintf(out_buf, max_len, " 0.0") > 0 ? 0 : -1;
    }

    char sign = steps < 0 ? '-' : '+';
    int abs_steps = steps < 0 ? -steps : steps;
    int whole = abs_steps / 3;
    int rem = abs_steps % 3;
    int frac = rem == 0 ? 0 : (rem == 1 ? 3 : 7);

    return snprintf(out_buf, max_len, "%c%d.%d", sign, whole, frac) > 0 ? 0 : -1;
}

const char *dml_camera_status_mode_name(DmlShootingMode mode) {
    switch (mode) {
        case DML_SHOOTING_MODE_MANUAL: return "M";
        case DML_SHOOTING_MODE_APERTURE_PRIORITY: return "A";
        case DML_SHOOTING_MODE_SHUTTER_PRIORITY: return "S";
        case DML_SHOOTING_MODE_PROGRAM: return "P";
        case DML_SHOOTING_MODE_AUTO: return "AUTO";
        case DML_SHOOTING_MODE_GUIDE: return "GUIDE";
        case DML_SHOOTING_MODE_EFFECTS: return "EFFECTS";
        default: return "?";
    }
}

const char *dml_camera_status_af_name(DmlAFMode af) {
    switch (af) {
        case DML_AF_MODE_AF_S: return "AF-S";
        case DML_AF_MODE_AF_C: return "AF-C";
        case DML_AF_MODE_AF_A: return "AF-A";
        case DML_AF_MODE_MF: return "MF";
        default: return "AF";
    }
}

const char *dml_camera_status_metering_name(DmlMeteringMode metering) {
    switch (metering) {
        case DML_METERING_MATRIX: return "Matrix";
        case DML_METERING_CENTER_WEIGHTED: return "Center";
        case DML_METERING_SPOT: return "Spot";
        default: return "Metering";
    }
}

const char *dml_camera_status_release_name(DmlReleaseMode release) {
    switch (release) {
        case DML_RELEASE_SINGLE: return "Single [S]";
        case DML_RELEASE_CONTINUOUS: return "Cont [C]";
        case DML_RELEASE_QUIET: return "Quiet [Q]";
        case DML_RELEASE_TIMER: return "Timer [10s]";
        default: return "Drive";
    }
}

const char *dml_camera_status_quality_name(DmlImageQuality quality) {
    switch (quality) {
        case DML_QUALITY_RAW_FINE: return "RAW+F";
        case DML_QUALITY_RAW: return "RAW";
        case DML_QUALITY_FINE: return "FINE";
        case DML_QUALITY_NORM: return "NORM";
        case DML_QUALITY_BASIC: return "BASIC";
        default: return "JPEG";
    }
}

const char *dml_camera_status_wb_name(DmlWhiteBalance wb) {
    switch (wb) {
        case DML_WB_AUTO: return "Auto";
        case DML_WB_INCANDESCENT: return "Incand";
        case DML_WB_FLUORESCENT: return "Fluor";
        case DML_WB_DIRECT_SUNLIGHT: return "Sun";
        case DML_WB_FLASH: return "Flash";
        case DML_WB_CLOUDY: return "Cloudy";
        case DML_WB_SHADE: return "Shade";
        case DML_WB_PRESET_MANUAL: return "Preset";
        default: return "WB";
    }
}

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../core/dml_camera_status_hw.h"

static void test_hw_status_unpacking(void) {
    DmlHwCameraStatusRaw mock_raw;
    memset(&mock_raw, 0, sizeof(mock_raw));

    mock_raw.vtable = D3300_ADDR_CKRRC_VTABLE;
    mock_raw.mode_flags = 0x00; /* M mode */
    mock_raw.tv_code = 40;       /* 1/250s */
    mock_raw.av_code = 9;        /* f/2.8 (280) */
    mock_raw.iso_code = 2;       /* ISO 400 */
    mock_raw.ev_compensation = 0;
    mock_raw.metering_mode = 0;  /* Matrix */
    mock_raw.af_mode = 0;        /* AF-S */
    mock_raw.active_focus_point = 0; /* Center */
    mock_raw.release_mode = 0;   /* Single */
    mock_raw.flash_state = 0x81; /* Raised + Fill flash (mode 1) */
    mock_raw.flash_compensation = 0;
    mock_raw.battery_status = 84;/* 84% */
    mock_raw.image_quality = 0;  /* RAW + Fine */
    mock_raw.shots_remaining = 1420;
    mock_raw.buffer_remaining = 12;
    mock_raw.white_balance = 0;  /* Auto */

    DmlCameraStatus status;
    int res = dml_camera_status_read_hw(&status, &mock_raw);
    assert(res == 0);

    assert(status.shooting_mode == DML_SHOOTING_MODE_MANUAL);
    assert(status.shutter_numerator == 1);
    assert(status.shutter_denominator == 250);
    assert(status.is_bulb == 0);
    assert(status.aperture_centi == 280);
    assert(status.iso == 400);
    assert(status.is_auto_iso == 0);
    assert(status.ev_compensation_third_stops == 0);
    assert(status.metering_mode == DML_METERING_MATRIX);
    assert(status.af_mode == DML_AF_MODE_AF_S);
    assert(status.active_focus_point == 0);
    assert(status.flash_raised == 1);
    assert(status.flash_mode == DML_FLASH_MODE_FILL);
    assert(status.battery_pct == 84);
    assert(status.battery_warning == 0);
    assert(status.image_quality == DML_QUALITY_RAW_FINE);
    assert(status.shots_remaining == 1420);
    assert(status.buffer_remaining == 12);
}

static void test_hw_status_edge_cases(void) {
    DmlHwCameraStatusRaw mock_raw;
    memset(&mock_raw, 0, sizeof(mock_raw));

    mock_raw.vtable = D3300_ADDR_CKRRC_VTABLE;
    mock_raw.mode_flags = 0x01; /* Aperture Priority (A) */
    mock_raw.tv_code = 0;        /* BULB */
    mock_raw.av_code = 30;       /* f/32 (3200) */
    mock_raw.iso_code = 0x88;    /* Auto ISO + index 8 (25600) */
    mock_raw.ev_compensation = -6; /* -2.0 EV */
    mock_raw.metering_mode = 2;  /* Spot */
    mock_raw.af_mode = 3;        /* MF */
    mock_raw.active_focus_point = 10; /* Outermost point */
    mock_raw.release_mode = 1;   /* Continuous */
    mock_raw.flash_state = 0x05; /* Lowered, Flash OFF */
    mock_raw.battery_status = 0x85; /* 5% + critical warning flag (bit 7) */
    mock_raw.image_quality = 1;  /* RAW */
    mock_raw.shots_remaining = 3;
    mock_raw.buffer_remaining = 0;
    mock_raw.white_balance = 3;  /* Direct sunlight */

    DmlCameraStatus status;
    int res = dml_camera_status_read_hw(&status, &mock_raw);
    assert(res == 0);

    assert(status.shooting_mode == DML_SHOOTING_MODE_APERTURE_PRIORITY);
    assert(status.is_bulb == 1);
    assert(status.aperture_centi == 3200);
    assert(status.iso == 25600);
    assert(status.is_auto_iso == 1);
    assert(status.ev_compensation_third_stops == -6);
    assert(status.metering_mode == DML_METERING_SPOT);
    assert(status.af_mode == DML_AF_MODE_MF);
    assert(status.active_focus_point == 10);
    assert(status.release_mode == DML_RELEASE_CONTINUOUS);
    assert(status.flash_raised == 0);
    assert(status.flash_mode == DML_FLASH_MODE_OFF);
    assert(status.battery_pct == 5);
    assert(status.battery_warning == 1);
    assert(status.image_quality == DML_QUALITY_RAW);
    assert(status.shots_remaining == 3);
    assert(status.buffer_remaining == 0);
    assert(status.white_balance == DML_WB_DIRECT_SUNLIGHT);
}

static void test_hw_status_validation(void) {
    DmlCameraStatus status;
    /* Null status output buffer */
    assert(dml_camera_status_read_hw(NULL, NULL) == -1);

    /* Zeroed vtable / uninitialized structure */
    DmlHwCameraStatusRaw uninit_raw;
    memset(&uninit_raw, 0, sizeof(uninit_raw));
    assert(dml_camera_status_read_hw(&status, &uninit_raw) == -1);
    uninit_raw.vtable = 0xDEADBEEFu;
    assert(dml_camera_status_read_hw(&status, &uninit_raw) == -1);

    /* Fixed camera RAM must never be dereferenced implicitly on any target. */
    assert(dml_camera_status_read_hw(&status, NULL) == -1);
}

int main(void) {
    test_hw_status_unpacking();
    test_hw_status_edge_cases();
    test_hw_status_validation();
    printf("All camera status HW tests passed successfully.\n");
    return 0;
}

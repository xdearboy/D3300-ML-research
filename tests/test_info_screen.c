#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/dml_camera_status.h"
#include "../core/dml_draw_buffer.h"
#include "../core/dml_info_screen.h"

static void test_status_formatting(void) {
    DmlCameraStatus status;
    dml_camera_status_init_default(&status);

    char buf[64];

    /* Default shutter: 1/250 */
    assert(dml_camera_status_format_shutter(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "1/250") == 0);

    /* Bulb */
    status.is_bulb = 1;
    assert(dml_camera_status_format_shutter(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "BULB") == 0);
    status.is_bulb = 0;

    /* Whole seconds: 2" */
    status.shutter_numerator = 2;
    status.shutter_denominator = 1;
    assert(dml_camera_status_format_shutter(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "2\"") == 0);

    /* High speed: 1/4000 */
    status.shutter_numerator = 1;
    status.shutter_denominator = 4000;
    assert(dml_camera_status_format_shutter(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "1/4000") == 0);

    /* Fractional seconds: 1.3" */
    status.shutter_numerator = 4;
    status.shutter_denominator = 3;
    assert(dml_camera_status_format_shutter(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "1.3\"") == 0);

    /* Aperture: default 280 -> F2.8 */
    assert(dml_camera_status_format_aperture(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "F2.8") == 0);

    status.aperture_centi = 140;
    assert(dml_camera_status_format_aperture(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "F1.4") == 0);

    status.aperture_centi = 2200;
    assert(dml_camera_status_format_aperture(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "F22") == 0);

    /* ISO */
    status.iso = 400;
    status.is_auto_iso = 0;
    assert(dml_camera_status_format_iso(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "400") == 0);

    status.is_auto_iso = 1;
    assert(dml_camera_status_format_iso(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "A400") == 0);

    status.is_auto_iso = 0;
    status.iso = 12800;
    assert(dml_camera_status_format_iso(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Hi 1") == 0);

    status.iso = 25600;
    assert(dml_camera_status_format_iso(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Hi 2") == 0);

    /* EV compensation */
    status.ev_compensation_third_stops = 0;
    assert(dml_camera_status_format_ev(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, " 0.0") == 0);

    status.ev_compensation_third_stops = 3; /* +1.0 EV */
    assert(dml_camera_status_format_ev(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "+1.0") == 0);

    status.ev_compensation_third_stops = -1; /* -0.3 EV */
    assert(dml_camera_status_format_ev(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "-0.3") == 0);

    status.ev_compensation_third_stops = -9; /* -3.0 EV */
    assert(dml_camera_status_format_ev(&status, buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "-3.0") == 0);

    /* Lookups */
    assert(strcmp(dml_camera_status_mode_name(DML_SHOOTING_MODE_MANUAL), "M") == 0);
    assert(strcmp(dml_camera_status_mode_name(DML_SHOOTING_MODE_APERTURE_PRIORITY), "A") == 0);
    assert(strcmp(dml_camera_status_af_name(DML_AF_MODE_AF_S), "AF-S") == 0);
    assert(strcmp(dml_camera_status_metering_name(DML_METERING_MATRIX), "Matrix") == 0);
    assert(strcmp(dml_camera_status_quality_name(DML_QUALITY_RAW_FINE), "RAW+F") == 0);
    assert(strcmp(dml_camera_status_wb_name(DML_WB_AUTO), "Auto") == 0);
}

static void test_draw_buffer_scaled_primitives(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    int idx = dml_draw_buffer_create(&table, 100, 100);
    assert(idx >= 0);

    /* Test line primitives */
    dml_draw_buffer_draw_line_h(&table, idx, 10, 10, 20, 0xFF112233u);
    uint32_t *pixels = table.entries[idx].pixels;
    uint32_t words_per_stride = table.entries[idx].stride / 4u;
    assert(pixels[10 * words_per_stride + 10] == 0xFF112233u);
    assert(pixels[10 * words_per_stride + 29] == 0xFF112233u);
    assert(pixels[10 * words_per_stride + 30] == 0x00000000u);

    /* Test scaled text */
    dml_draw_buffer_draw_text_scaled(&table, idx, 40, 40, "X", 0xFFAABBCCu, 0x00000000u, 2);
    /* At scale 2, glyph is 16x16 pixels */
    int found_pixel = 0;
    for (int y = 40; y < 56; y++) {
        for (int x = 40; x < 56; x++) {
            if (pixels[y * words_per_stride + x] == 0xFFAABBCCu) {
                found_pixel = 1;
                break;
            }
        }
    }
    assert(found_pixel == 1);

    dml_draw_buffer_cleanup(&table);
}

static void test_info_screen_render(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    int idx = dml_draw_buffer_create(&table, 720, 480);
    assert(idx >= 0);

    DmlCameraStatus status;
    dml_camera_status_init_default(&status);
    status.exposure_locked = 1;
    status.focus_locked = 1;
    status.battery_pct = 75;

    /* Error handling */
    assert(dml_info_screen_render(NULL, idx, &status) == -1);
    assert(dml_info_screen_render(&table, -1, &status) == -1);
    assert(dml_info_screen_render(&table, 99, &status) == -1);
    assert(dml_info_screen_render(&table, idx, NULL) == -1);

    /* Valid render */
    int res = dml_info_screen_render(&table, idx, &status);
    assert(res == 0);

    /* Validate frame content: pixels must be colored */
    const DmlDrawBufferEntry *entry = &table.entries[idx];
    uint32_t words_per_stride = entry->stride / 4u;

    /* Background at (2, 2) should be DML_COLOR_BG */
    assert(entry->pixels[2 * words_per_stride + 2] == DML_COLOR_BG);

    /* Shooting mode indicator around (28..36, 20..28) should contain mauve text pixel */
    int mode_has_mauve = 0;
    for (int y = 20; y < 28; y++) {
        for (int x = 28; x < 36; x++) {
            if (entry->pixels[y * words_per_stride + x] == DML_COLOR_ACCENT_MAUVE) {
                mode_has_mauve = 1;
                break;
            }
        }
    }
    assert(mode_has_mauve == 1);

    /* Shutter hero section around (40..150, 106..165) should have stroke pixels */
    int card_has_content = 0;
    for (int y = 106; y < 165; y++) {
        for (int x = 40; x < 150; x++) {
            if (entry->pixels[y * words_per_stride + x] == DML_COLOR_TEXT_PRIMARY) {
                card_has_content = 1;
                break;
            }
        }
    }
    assert(card_has_content == 1);

    /* Test exporting to BMP */
    const char *bmp_path = "build/test_info_screen.bmp";
    assert(dml_draw_buffer_save_bmp(&table, idx, bmp_path) == 0);

    FILE *fp = fopen(bmp_path, "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    /* 720x480 24bpp BMP: 54 header + (720*3)*480 = 54 + 1,036,800 = 1,036,854 bytes */
    assert(sz == 1036854L);

    dml_draw_buffer_cleanup(&table);
}

int main(void) {
    test_status_formatting();
    test_draw_buffer_scaled_primitives();
    test_info_screen_render();
    puts("All D3300-ML custom info screen tests passed successfully.");
    return 0;
}

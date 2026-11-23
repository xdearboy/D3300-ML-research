#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../core/dml_draw_buffer.h"

static void test_stride_calculation(void) {
    /* Formula: ((w + 7) & ~7) * 4 */
    assert(DML_DRAW_BUFFER_STRIDE(720) == 720 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(1920) == 1920 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(960) == 960 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(1) == 8 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(7) == 8 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(8) == 8 * 4);
    assert(DML_DRAW_BUFFER_STRIDE(9) == 16 * 4);
}

static void test_table_lifecycle(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    assert(table.current_selected == -1);

    /* Create 720x480 buffer (standard D3300 LCD/OSD resolution) */
    int idx0 = dml_draw_buffer_create(&table, 720, 480);
    assert(idx0 == 0);
    assert(table.entries[0].active == 1);
    assert(table.entries[0].width == 720);
    assert(table.entries[0].height == 480);
    assert(table.entries[0].stride == 720 * 4);
    assert(table.entries[0].pixels != NULL);

    /* Select buffer */
    assert(dml_draw_buffer_select(&table, idx0) == 0);
    assert(table.current_selected == idx0);

    /* Query tests */
    uint16_t w = 0, h = 0;
    uint32_t stride = 0, fmt = 0, sync = 0;
    uintptr_t addr = 0;

    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_WIDTH, &w) == 0 && w == 720);
    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_HEIGHT, &h) == 0 && h == 480);
    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_STRIDE, &stride) == 0 && stride == 720 * 4);
    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_FORMAT, &fmt) == 0 && fmt == 4);
    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_SYNC, &sync) == 0 && sync == 1);
    assert(dml_draw_buffer_query(&table, idx0, DML_DRAW_BUFFER_QUERY_ADDR, &addr) == 0 && addr == (uintptr_t)table.entries[0].pixels);

    /* Swap */
    assert(dml_draw_buffer_swap(&table, idx0, 1) == 0);

    /* Fill all 20 slots */
    for (int i = 1; i < 20; i++) {
        int idx = dml_draw_buffer_create(&table, 100, 100);
        assert(idx == i);
    }
    /* 21st slot should fail */
    int overflow = dml_draw_buffer_create(&table, 100, 100);
    assert(overflow == -1);

    /* Destroy buffer 0 (which was selected) */
    assert(dml_draw_buffer_destroy(&table, 0) == 0);
    assert(table.entries[0].active == 0);
    assert(table.entries[0].pixels == NULL);
    assert(table.current_selected == -1);

    /* Slot 0 is now reusable */
    int reused = dml_draw_buffer_create(&table, 320, 240);
    assert(reused == 0);

    dml_draw_buffer_cleanup(&table);
    assert(table.current_selected == -1);
}

static void test_rasterizer(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    int idx = dml_draw_buffer_create(&table, 64, 64);
    assert(idx >= 0);

    /* Clear to red */
    dml_draw_buffer_clear(&table, idx, 0xFFFF0000u);
    assert(table.entries[idx].pixels[0] == 0xFFFF0000u);
    assert(table.entries[idx].pixels[63 * (table.entries[idx].stride / 4) + 63] == 0xFFFF0000u);

    /* Fill rect in green at (10, 10, 20, 20) */
    dml_draw_buffer_fill_rect(&table, idx, 10, 10, 20, 20, 0xFF00FF00u);
    uint32_t stride_words = table.entries[idx].stride / 4u;
    assert(table.entries[idx].pixels[9 * stride_words + 9] == 0xFFFF0000u);
    assert(table.entries[idx].pixels[10 * stride_words + 10] == 0xFF00FF00u);
    assert(table.entries[idx].pixels[29 * stride_words + 29] == 0xFF00FF00u);
    assert(table.entries[idx].pixels[30 * stride_words + 30] == 0xFFFF0000u);

    /* Draw char 'A' at (0, 0) */
    dml_draw_buffer_draw_char(&table, idx, 0, 0, 'A', 0xFFFFFFFFu, 0xFF000000u);
    /* In 8x8 font, row 0 of 'A' is 0x38 (00111000) -> cols 2, 3, 4 are set */
    assert(table.entries[idx].pixels[0 * stride_words + 0] == 0xFF000000u);
    assert(table.entries[idx].pixels[0 * stride_words + 1] == 0xFF000000u);
    assert(table.entries[idx].pixels[0 * stride_words + 2] == 0xFFFFFFFFu);
    assert(table.entries[idx].pixels[0 * stride_words + 3] == 0xFFFFFFFFu);
    assert(table.entries[idx].pixels[0 * stride_words + 4] == 0xFFFFFFFFu);

    /* Draw text */
    dml_draw_buffer_draw_text(&table, idx, 0, 32, "D3300", 0xFF0000FFu, 0x00000000u);
    /* Should not crash and should render characters */

    dml_draw_buffer_cleanup(&table);
}

static void test_screenshot_export(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    /* Negative tests */
    assert(dml_draw_buffer_save_bmp(NULL, 0, "test.bmp") == -1);
    assert(dml_draw_buffer_save_bmp(&table, -1, "test.bmp") == -1);
    assert(dml_draw_buffer_save_bmp(&table, 0, NULL) == -1);
    assert(dml_draw_buffer_save_bmp(&table, 0, "test.bmp") == -1); /* not active */

    assert(dml_draw_buffer_save_ppm(NULL, 0, "test.ppm") == -1);
    assert(dml_draw_buffer_save_ppm(&table, -1, "test.ppm") == -1);
    assert(dml_draw_buffer_save_ppm(&table, 0, NULL) == -1);
    assert(dml_draw_buffer_save_ppm(&table, 0, "test.ppm") == -1); /* not active */

    int idx = dml_draw_buffer_create(&table, 64, 32);
    assert(idx >= 0);

    /* Render pattern */
    dml_draw_buffer_clear(&table, idx, 0xFF102030u);
    dml_draw_buffer_fill_rect(&table, idx, 4, 4, 16, 16, 0xFFFF0000u);
    dml_draw_buffer_draw_text(&table, idx, 24, 8, "TEST", 0xFF00FF00u, 0x00000000u);

    const char *bmp_path = "/tmp/test_d3300_screenshot.bmp";
    const char *ppm_path = "/tmp/test_d3300_screenshot.ppm";

    assert(dml_draw_buffer_save_bmp(&table, idx, bmp_path) == 0);
    assert(dml_draw_buffer_save_ppm(&table, idx, ppm_path) == 0);

    /* Verify BMP header */
    FILE *fbmp = fopen(bmp_path, "rb");
    assert(fbmp != NULL);
    uint8_t bmp_hdr[54];
    assert(fread(bmp_hdr, 1, 54, fbmp) == 54);
    assert(bmp_hdr[0] == 'B' && bmp_hdr[1] == 'M');
    uint32_t bmp_width = bmp_hdr[18] | (bmp_hdr[19] << 8) | (bmp_hdr[20] << 16) | (bmp_hdr[21] << 24);
    uint32_t bmp_height = bmp_hdr[22] | (bmp_hdr[23] << 8) | (bmp_hdr[24] << 16) | (bmp_hdr[25] << 24);
    uint16_t bmp_bpp = bmp_hdr[28] | (bmp_hdr[29] << 8);
    assert(bmp_width == 64);
    assert(bmp_height == 32);
    assert(bmp_bpp == 24);
    fclose(fbmp);
    remove(bmp_path);

    /* Verify PPM header */
    FILE *fppm = fopen(ppm_path, "rb");
    assert(fppm != NULL);
    char ppm_hdr[32];
    assert(fgets(ppm_hdr, sizeof(ppm_hdr), fppm) != NULL);
    assert(strcmp(ppm_hdr, "P6\n") == 0);
    assert(fgets(ppm_hdr, sizeof(ppm_hdr), fppm) != NULL);
    assert(strcmp(ppm_hdr, "64 32\n") == 0);
    assert(fgets(ppm_hdr, sizeof(ppm_hdr), fppm) != NULL);
    assert(strcmp(ppm_hdr, "255\n") == 0);
    fclose(fppm);
    remove(ppm_path);

    dml_draw_buffer_cleanup(&table);
}

int main(void) {
    test_stride_calculation();
    test_table_lifecycle();
    test_rasterizer();
    test_screenshot_export();
    puts("All DrawBuffer tests passed successfully.");
    return 0;
}


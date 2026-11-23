#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/dml_draw_buffer.h"
#include "../core/dml_menu.h"
#include "../modules/photo_assist.h"

static void test_photo_assist_enums_and_names(void) {
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_OFF), "OFF") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_GREEN), "GREEN") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_RED), "RED") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_CYAN), "CYAN") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_YELLOW), "YELLOW") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_color_name(DML_PEAK_WHITE), "WHITE") == 0);

    assert(strcmp(dml_photo_assist_get_peaking_sens_name(DML_PEAK_SENS_LOW), "LOW") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_sens_name(DML_PEAK_SENS_MED), "MEDIUM") == 0);
    assert(strcmp(dml_photo_assist_get_peaking_sens_name(DML_PEAK_SENS_HIGH), "HIGH") == 0);

    assert(strcmp(dml_photo_assist_get_zebra_level_name(DML_ZEBRA_OFF), "OFF") == 0);
    assert(strcmp(dml_photo_assist_get_zebra_level_name(DML_ZEBRA_100), "100%") == 0);
    assert(strcmp(dml_photo_assist_get_zebra_level_name(DML_ZEBRA_95), "95%") == 0);
    assert(strcmp(dml_photo_assist_get_zebra_level_name(DML_ZEBRA_90), "90%") == 0);
    assert(strcmp(dml_photo_assist_get_zebra_level_name(DML_ZEBRA_75), "75%") == 0);

    assert(strcmp(dml_photo_assist_get_histogram_mode_name(DML_HISTO_OFF), "OFF") == 0);
    assert(strcmp(dml_photo_assist_get_histogram_mode_name(DML_HISTO_LUMA), "LUMA") == 0);
    assert(strcmp(dml_photo_assist_get_histogram_mode_name(DML_HISTO_RGB), "RGB") == 0);

    assert(dml_photo_assist_get_peaking_rgb(DML_PEAK_GREEN) == 0x00FF66u);
    assert(dml_photo_assist_get_peaking_rgb(DML_PEAK_RED) == 0xFF2244u);
}

static void test_focus_peaking_edge_detection(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);
    int buf = dml_draw_buffer_create(&table, 100, 100);
    assert(buf >= 0);

    /* Construct a 100x100 synthetic frame with a sharp vertical edge at x=50 */
    uint8_t luma[100 * 100];
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            luma[y * 100 + x] = (x < 50) ? 20 : 220;
        }
    }

    /* Clear drawing buffer */
    dml_draw_buffer_fill_rect(&table, buf, 0, 0, 100, 100, 0x000000u);

    /* Apply green focus peaking */
    dml_photo_assist_apply_peaking(&table, buf, luma, 100, 100, DML_PEAK_GREEN, DML_PEAK_SENS_MED);

    /* Check edge pixels around x=49 and x=50 are highlighted */
    const DmlDrawBufferEntry *dbuf = &table.entries[buf];
    assert(dbuf->pixels != NULL);
    uint32_t stride_words = dbuf->stride / 4u;

    int edge_hits = 0;
    for (int y = 5; y < 95; y++) {
        uint32_t c49 = dbuf->pixels[y * stride_words + 49];
        uint32_t c50 = dbuf->pixels[y * stride_words + 50];
        if (c49 == 0x00FF66u || c50 == 0x00FF66u) {
            edge_hits++;
        }
    }
    assert(edge_hits > 80); /* Sharp vertical boundary detected along column */

    /* Uniform region should have zero peaking hits */
    for (int y = 5; y < 95; y++) {
        assert(dbuf->pixels[y * stride_words + 10] == 0x000000u);
        assert(dbuf->pixels[y * stride_words + 90] == 0x000000u);
    }

    dml_draw_buffer_cleanup(&table);
}

static void test_zebra_overexposure_thresholding(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);
    int buf = dml_draw_buffer_create(&table, 80, 80);
    assert(buf >= 0);

    /* Construct frame: left side safe (128), right side blown highlight (255) */
    uint8_t luma[80 * 80];
    for (int y = 0; y < 80; y++) {
        for (int x = 0; x < 80; x++) {
            luma[y * 80 + x] = (x < 40) ? 128 : 255;
        }
    }

    dml_draw_buffer_fill_rect(&table, buf, 0, 0, 80, 80, 0x000000u);
    dml_photo_assist_apply_zebra(&table, buf, luma, 80, 80, DML_ZEBRA_100, 0);

    const DmlDrawBufferEntry *dbuf = &table.entries[buf];
    assert(dbuf->pixels != NULL);
    uint32_t stride_words = dbuf->stride / 4u;

    /* Left half should have zero zebra stripes (safe luma) */
    for (int y = 0; y < 80; y++) {
        for (int x = 0; x < 35; x++) {
            assert(dbuf->pixels[y * stride_words + x] == 0x000000u);
        }
    }

    /* Right half must contain zebra pattern */
    int zebra_hits = 0;
    for (int y = 0; y < 80; y++) {
        for (int x = 45; x < 80; x++) {
            uint32_t c = dbuf->pixels[y * stride_words + x];
            if (c == 0x111118u || c == 0xFF3333u) {
                zebra_hits++;
            }
        }
    }
    assert(zebra_hits > 1000);

    dml_draw_buffer_cleanup(&table);
}

static void test_histogram_rendering(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);
    int buf = dml_draw_buffer_create(&table, 200, 100);
    assert(buf >= 0);

    uint8_t luma[200 * 100];
    memset(luma, 128, sizeof(luma));

    dml_draw_buffer_fill_rect(&table, buf, 0, 0, 200, 100, 0x000000u);
    dml_photo_assist_draw_histogram(&table, buf, luma, 200, 100, 10, 10, DML_HISTO_LUMA);

    const DmlDrawBufferEntry *dbuf = &table.entries[buf];
    assert(dbuf->pixels != NULL);
    uint32_t stride_words = dbuf->stride / 4u;

    /* Border check at (10, 10) */
    assert(dbuf->pixels[10 * stride_words + 10] == 0x3E3E55u);

    dml_draw_buffer_cleanup(&table);
}

static void test_menu_photo_assist_cycling(void) {
    DmlMenu menu = {0};

    /* Peaking color */
    assert(menu.peaking_color == 0);
    dml_menu_cycle_peaking_color(&menu);
    assert(menu.peaking_color == 1);
    assert(strcmp(dml_menu_get_peaking_color_name(menu.peaking_color), "GREEN") == 0);

    /* Peaking sensitivity */
    assert(menu.peaking_sens == 0);
    dml_menu_cycle_peaking_sens(&menu);
    assert(menu.peaking_sens == 1);
    assert(strcmp(dml_menu_get_peaking_sens_name(menu.peaking_sens), "MEDIUM") == 0);

    /* Zebra level */
    assert(menu.zebra_level == 0);
    dml_menu_cycle_zebra_level(&menu);
    assert(menu.zebra_level == 1);
    assert(strcmp(dml_menu_get_zebra_level_name(menu.zebra_level), "100%") == 0);

    /* Histogram mode */
    assert(menu.histogram_mode == 0);
    dml_menu_cycle_histogram_mode(&menu);
    assert(menu.histogram_mode == 1);
    assert(strcmp(dml_menu_get_histogram_mode_name(menu.histogram_mode), "LUMA") == 0);
}

int main(void) {
    test_photo_assist_enums_and_names();
    test_focus_peaking_edge_detection();
    test_zebra_overexposure_thresholding();
    test_histogram_rendering();
    test_menu_photo_assist_cycling();
    printf("All Photo Assist tests passed successfully!\n");
    return 0;
}

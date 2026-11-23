#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../core/dml_draw_buffer.h"
#include "../core/dml_menu.h"
#include "../core/dml_camera_status.h"
#include "../core/dml_menu_screen.h"

static void dummy_action(void *state, uint32_t millis, const DmlCameraAPI *api) {
    (void)state;
    (void)millis;
    (void)api;
}

static void test_menu_screen_rendering(void) {
    DmlDrawBufferTable table;
    dml_draw_buffer_init(&table);

    int buf_idx = dml_draw_buffer_create(&table, 720, 480);
    assert(buf_idx >= 0);
    dml_draw_buffer_select(&table, buf_idx);

    DmlMenu menu = {0};
    menu.visible = 1;
    menu.active_tab = DML_TAB_SHOOT;
    menu.selected = 0;

    assert(dml_menu_add(&menu, (DmlMenuItem){
        .label = "INTERVALOMETER",
        .desc = "Automated timelapse shutter sequence",
        .tab = DML_TAB_SHOOT,
        .value_text = "1.0s",
        .status_text = "ON",
        .is_on = 1,
        .has_val = 1,
        .activate = dummy_action
    }));

    assert(dml_menu_add(&menu, (DmlMenuItem){
        .label = "BURST RATE LIMIT",
        .desc = "Continuous shooting frequency",
        .tab = DML_TAB_SHOOT,
        .value_text = "5.0 FPS",
        .has_val = 1,
        .activate = dummy_action
    }));

    assert(dml_menu_add(&menu, (DmlMenuItem){
        .label = "AUTO-ISO MAXIMUM",
        .desc = "Sensitivity limit",
        .tab = DML_TAB_EXPO,
        .value_text = "6400",
        .has_val = 1,
        .activate = dummy_action
    }));

    DmlCameraStatus status;
    dml_camera_status_init_default(&status);
    status.battery_pct = 84;

    /* Render flat cinema menu */
    int res = dml_menu_screen_render(&table, buf_idx, &menu, &status);
    assert(res == 0);

    const DmlDrawBufferEntry *entry = &table.entries[buf_idx];
    assert(entry->pixels != NULL);

    /* Verify top bar color (0xFF13131D) */
    uint32_t top_pixel = entry->pixels[10 * entry->stride / 4 + 10];
    assert(top_pixel == 0xFF13131Du);

    /* Verify selected row left accent stripe (Emerald 0xFF00BB7F at x=2, y=50) */
    uint32_t stripe_pixel = entry->pixels[50 * entry->stride / 4 + 2];
    assert(stripe_pixel == 0xFF00BB7Fu);

    /* Verify selected row background (0xFF222336 at x=20, y=50) */
    uint32_t row_bg_pixel = entry->pixels[50 * entry->stride / 4 + 20];
    assert(row_bg_pixel == 0xFF222336u);

    /* Verify footer background (0xFF12121B at x=10, y=450) */
    uint32_t footer_pixel = entry->pixels[450 * entry->stride / 4 + 10];
    assert(footer_pixel == 0xFF12121Bu);

    /* Test tab navigation */
    assert(dml_menu_handle(&menu, DML_MENU_NEXT_TAB, 0, NULL) == 1);
    assert(menu.active_tab == DML_TAB_EXPO);
    assert(menu.selected == 0);

    res = dml_menu_screen_render(&table, buf_idx, &menu, &status);
    assert(res == 0);

    /* Parameter validation checks */
    assert(dml_menu_screen_render(NULL, buf_idx, &menu, &status) == -1);
    assert(dml_menu_screen_render(&table, -1, &menu, &status) == -1);
    assert(dml_menu_screen_render(&table, buf_idx, NULL, &status) == -1);

    dml_draw_buffer_cleanup(&table);
}

int main(void) {
    test_menu_screen_rendering();
    printf("All D3300-ML menu screen tests passed successfully.\n");
    return 0;
}

#include <assert.h>
#include "../core/dml_menu.h"
#include "../modules/intervalometer.h"

static void test_menu_selection(void) {
    DmlMenu menu = {0};
    DmlIntervalometer first, second;
    dml_intervalometer_config(&first, 1000);
    dml_intervalometer_config(&second, 2000);
    assert(!dml_menu_handle(&menu, DML_MENU_ACTIVATE, 0, NULL));
    assert(dml_menu_handle(&menu, DML_MENU_TOGGLE, 0, NULL));
    assert(dml_menu_handle(&menu, DML_MENU_NEXT, 0, NULL));
    assert(dml_menu_handle(&menu, DML_MENU_ACTIVATE, 0, NULL));
    assert(menu.selected == 0);
    assert(!dml_menu_add(&menu, (DmlMenuItem){0}));
    assert(dml_menu_add(&menu, (DmlMenuItem){
        .label = "First",
        .state = &first,
        .activate = dml_intervalometer_toggle
    }));
    assert(dml_menu_add(&menu, (DmlMenuItem){
        .label = "Second",
        .state = &second,
        .activate = dml_intervalometer_toggle
    }));
    assert(dml_menu_handle(&menu, DML_MENU_PREVIOUS, 10, NULL));
    assert(menu.selected == 1);
    assert(dml_menu_handle(&menu, DML_MENU_ACTIVATE, 10, NULL));
    assert(!first.active && second.active && second.next_due_ms == 2010);
    assert(dml_menu_handle(&menu, DML_MENU_NEXT, 10, NULL));
    assert(menu.selected == 0);
    assert(dml_menu_handle(&menu, DML_MENU_TOGGLE, 10, NULL));
    assert(!dml_menu_handle(&menu, DML_MENU_ACTIVATE, 10, NULL));
    for (size_t i = 2; i < DML_MENU_MAX_ITEMS; ++i) {
        assert(dml_menu_add(&menu, (DmlMenuItem){
            .label = "More",
            .state = &first,
            .activate = dml_intervalometer_toggle
        }));
    }
    assert(!dml_menu_add(&menu, (DmlMenuItem){
        .label = "Full",
        .state = &first,
        .activate = dml_intervalometer_toggle
    }));
}

static void test_independent_modules(void) {
    DmlRuntime runtime;
    DmlIntervalometer first, second;
    DmlEvent key = {DML_EVENT_BUTTON, 100, 'i'};
    dml_init(&runtime, NULL);
    dml_intervalometer_config(&first, 1000);
    dml_intervalometer_config(&second, 2000);
    assert(dml_register(&runtime, dml_intervalometer_module(&first)));
    assert(dml_register(&runtime, dml_intervalometer_module(&second)));
    assert(!dml_register(&runtime, dml_intervalometer_module(&first)));
    dml_dispatch(&runtime, &key);
    assert(first.active && first.next_due_ms == 1100);
    assert(second.active && second.next_due_ms == 2100);
}

static void test_menu_long_press_and_animation(void) {
    DmlMenu menu = {0};
    DmlMenuCommand cmd = 0;

    /* 1. Short tap: press down at t=100 */
    assert(dml_menu_on_menu_button(&menu, 1, 100, &cmd) == 0);
    assert(menu.is_holding_menu == 1);
    assert(menu.hold_progress == 0);

    /* Release at t=250 (<500ms): short tap triggers factory passthrough! */
    assert(dml_menu_on_menu_button(&menu, 0, 250, &cmd) == 1);
    assert(cmd == DML_MENU_PASSTHROUGH_FACTORY);
    assert(menu.is_holding_menu == 0);
    assert(menu.visible == 0);

    /* 2. Long press: press down at t=1000 */
    assert(dml_menu_on_menu_button(&menu, 1, 1000, &cmd) == 0);
    assert(menu.is_holding_menu == 1);

    /* Advance ticks */
    dml_menu_tick(&menu, 1250);
    assert(menu.hold_progress == 50);
    assert(menu.visible == 0);

    /* Hit 500ms threshold at t=1500 */
    dml_menu_tick(&menu, 1500);
    assert(menu.hold_progress == 100);
    assert(menu.visible == 1);
    assert(menu.is_holding_menu == 0);
    assert(menu.anim_progress == 0);

    /* Advance animation ticks */
    dml_menu_tick(&menu, 1520);
    assert(menu.anim_progress == 25);
    dml_menu_tick(&menu, 1540);
    assert(menu.anim_progress == 50);
    dml_menu_tick(&menu, 1580);
    assert(menu.anim_progress == 75);
    dml_menu_tick(&menu, 1600);
    assert(menu.anim_progress == 100);

    /* 3. When menu is open, pressing MENU closes it */
    assert(dml_menu_on_menu_button(&menu, 1, 2000, &cmd) == 1);
    assert(cmd == DML_MENU_TOGGLE);
    dml_menu_handle(&menu, cmd, 2000, NULL);
    assert(menu.visible == 0);
}

int main(void) {
    test_menu_selection();
    test_independent_modules();
    test_menu_long_press_and_animation();
    return 0;
}

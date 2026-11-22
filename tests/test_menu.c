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
    assert(!dml_menu_add(&menu, (DmlMenuItem) {0}));
    assert(dml_menu_add(&menu, (DmlMenuItem) {"First", &first, dml_intervalometer_toggle}));
    assert(dml_menu_add(&menu, (DmlMenuItem) {"Second", &second, dml_intervalometer_toggle}));
    assert(dml_menu_handle(&menu, DML_MENU_PREVIOUS, 10, NULL));
    assert(menu.selected == 1);
    assert(dml_menu_handle(&menu, DML_MENU_ACTIVATE, 10, NULL));
    assert(!first.active && second.active && second.next_due_ms == 2010);
    assert(dml_menu_handle(&menu, DML_MENU_NEXT, 10, NULL));
    assert(menu.selected == 0);
    assert(dml_menu_handle(&menu, DML_MENU_TOGGLE, 10, NULL));
    assert(!dml_menu_handle(&menu, DML_MENU_ACTIVATE, 10, NULL));
    for (size_t i = 2; i < DML_MENU_MAX_ITEMS; ++i) {
        assert(dml_menu_add(&menu, (DmlMenuItem) {"More", &first, dml_intervalometer_toggle}));
    }
    assert(!dml_menu_add(&menu, (DmlMenuItem) {"Full", &first, dml_intervalometer_toggle}));
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

int main(void) {
    test_menu_selection();
    test_independent_modules();
}

#include <assert.h>
#include <stdio.h>

#include "../core/dml_ui_logical_event.h"

static void test_proven_logical_routes(void) {
    DmlUiLogicalRoute route = {0};

    assert(dml_ui_logical_route_lookup(0x01u, &route) == 1);
    assert(route.vtable_slot_offset == 0x12Cu);
    assert(route.rom_target == 0x90076FF4u);

    assert(dml_ui_logical_route_lookup(0x05u, &route) == 1);
    assert(route.vtable_slot_offset == 0x130u);
    assert(route.rom_target == 0x90077512u);

    assert(dml_ui_logical_route_lookup(0x1Bu, &route) == 1);
    assert(route.vtable_slot_offset == 0x17Cu);
    assert(route.rom_target == 0x90077CBDu);
}

static void test_unknown_or_invalid_lookup(void) {
    DmlUiLogicalRoute route = {0};
    assert(dml_ui_logical_route_lookup(0x00u, &route) == 0);
    assert(dml_ui_logical_route_lookup(0x1Cu, &route) == 0);
    assert(dml_ui_logical_route_lookup(0x01u, NULL) == 0);
}

int main(void) {
    test_proven_logical_routes();
    test_unknown_or_invalid_lookup();
    puts("All host-only logical UI route tests passed.");
    return 0;
}

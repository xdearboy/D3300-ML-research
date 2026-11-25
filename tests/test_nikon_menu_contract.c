#include <assert.h>
#include <stdio.h>

#include "../core/dml_nikon_menu_contract.h"

static void test_confirmed_raw_to_logical_translation(void) {
    DmlNikonButtonRecord record = {0};
    DmlNikonMenuLogicalEvent event = {0};

    record.code = 0x000Bu;
    assert(dml_nikon_menu_logical_event(&record, &event) == 1);
    assert(event.raw_code == 0x000Bu);
    assert(event.logical_id == 0x1Bu);

    record.code = 0x000Cu;
    record.value_1 = 0u;
    assert(dml_nikon_menu_logical_event(&record, &event) == 1);
    assert(event.logical_id == 0x01u);

    record.code = 0x0010u;
    assert(dml_nikon_menu_logical_event(&record, &event) == 1);
    assert(event.logical_id == 0x05u);
}

static void test_unmapped_and_out_of_range_are_rejected(void) {
    DmlNikonButtonRecord record = {0};
    DmlNikonMenuLogicalEvent event = {0};

    record.code = 0x0008u;
    assert(dml_nikon_menu_logical_event(&record, &event) == 0);
    record.code = 0x003Bu;
    assert(dml_nikon_menu_logical_event(&record, &event) == 0);
    assert(dml_nikon_menu_logical_event(NULL, &event) == 0);
    assert(dml_nikon_menu_logical_event(&record, NULL) == 0);
}

int main(void) {
    test_confirmed_raw_to_logical_translation();
    test_unmapped_and_out_of_range_are_rejected();
    puts("All Nikon menu-contract tests passed.");
    return 0;
}

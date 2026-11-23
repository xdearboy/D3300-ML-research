#include <assert.h>
#include <stdint.h>

#include "../core/dml_camera_status.h"
#include "../core/dml_draw_buffer.h"
#include "../core/dml_menu.h"
#include "../core/dml_menu_screen.h"
#include "../core/dml_nikon_adapter.h"

static void test_candidate_button_to_menu_and_render(void) {
    DmlMenu menu = {0};
    DmlCameraStatus status;
    DmlDrawBufferTable buffers;
    DmlNikonButtonRecord record = {0};
    DmlMenuCommand command = 0;
    DmlEvent event = {0};

    dml_camera_status_init_default(&status);
    dml_draw_buffer_init(&buffers);

    /* These numeric button values are candidate mappings, not claimed camera ABI. */
    record.code = DML_NIKON_BTN_CODE_MENU_CANDIDATE;
    record.value_1 = 1u;
    assert(dml_nikon_translate_record(&record, &command, &event, 100u) == 1);
    assert(command == DML_MENU_TOGGLE);
    assert(event.type == DML_EVENT_BUTTON && event.button == (int)record.code);
    assert(dml_menu_handle(&menu, command, event.millis, NULL) == 1);
    assert(menu.visible == 1);

    record.code = DML_NIKON_BTN_CODE_RIGHT_CANDIDATE;
    assert(dml_nikon_translate_record(&record, &command, &event, 120u) == 1);
    assert(command == DML_MENU_NEXT_TAB);
    assert(dml_menu_handle(&menu, command, event.millis, NULL) == 1);
    assert(menu.active_tab == DML_TAB_EXPO);

    record.code = DML_NIKON_BTN_CODE_INFO_CANDIDATE;
    assert(dml_nikon_translate_record(&record, &command, &event, 140u) == 1);
    assert(command == DML_MENU_TOGGLE_INFO);

    int buffer = dml_draw_buffer_create(&buffers, 720u, 480u);
    assert(buffer >= 0);
    assert(dml_menu_screen_render(&buffers, buffer, &menu, &status) == 0);
    assert(buffers.entries[buffer].pixels != NULL);

    dml_draw_buffer_cleanup(&buffers);
}

int main(void) {
    test_candidate_button_to_menu_and_render();
    return 0;
}

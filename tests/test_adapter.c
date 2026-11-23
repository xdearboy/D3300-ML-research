#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
#include "../core/dml_nikon_adapter.h"

static int g_relay_calls = 0;
static uint32_t g_last_mask = 0;

static void test_relay_callback(uint32_t mask, void *context) {
    g_relay_calls++;
    g_last_mask = mask;
    int *val = (int *)context;
    if (val) {
        (*val)++;
    }
}

static void test_button_state_rw(void) {
    DmlNikonButtonState state;
    memset(&state, 0, sizeof(state));

    DmlNikonButtonRecord rec_in = {
        .code = 0x000C,
        .object_id = 0x0008,
        .value_1 = 1,
        .value_2 = 0x12345678,
        .queue_mask = 0x80000001,
    };

    /* Initial state is empty */
    DmlNikonButtonRecord rec_out;
    memset(&rec_out, 0, sizeof(rec_out));
    assert(dml_nikon_button_state_read(&state, &rec_out) == 0);

    /* Write record */
    dml_nikon_button_state_write(&state, &rec_in);
    assert(state.ready == 1);

    /* Poll without reading buffer */
    assert(dml_nikon_button_state_read(&state, NULL) == 1);

    /* Read buffer */
    assert(dml_nikon_button_state_read(&state, &rec_out) == 1);
    assert(rec_out.code == 0x000C);
    assert(rec_out.object_id == 0x0008);
    assert(rec_out.value_1 == 1);
    assert(rec_out.value_2 == 0x12345678);
    assert(rec_out.queue_mask == 0x80000001);
}

static void test_relay_table(void) {
    DmlNikonRelayTable table;
    dml_nikon_relay_init(&table);

    int context_val = 10;
    g_relay_calls = 0;
    g_last_mask = 0;

    /* Register callback 1 */
    uint32_t token1 = dml_nikon_relay_register(&table, test_relay_callback, &context_val);
    assert(token1 == 1u); /* slot 0 -> mask 0x1 */

    /* Dispatch non-matching event mask */
    dml_nikon_relay_dispatch(&table, 0x00000002);
    assert(g_relay_calls == 0);
    assert(context_val == 10);

    /* Dispatch matching event mask */
    dml_nikon_relay_dispatch(&table, 0x00000001);
    assert(g_relay_calls == 1);
    assert(g_last_mask == 0x00000001);
    assert(context_val == 11);

    /* Confirm persistent callback: second dispatch also invokes */
    dml_nikon_relay_dispatch(&table, 0x00000003);
    assert(g_relay_calls == 2);
    assert(context_val == 12);

    /* Unregister */
    assert(dml_nikon_relay_unregister(&table, token1) == 1);
    assert(dml_nikon_relay_unregister(&table, token1) == 0); /* second unregister fails */

    /* Subsequent dispatch does not invoke */
    dml_nikon_relay_dispatch(&table, 0x00000001);
    assert(g_relay_calls == 2);

    /* Test table overflow */
    uint32_t tokens[32];
    for (int i = 0; i < 32; i++) {
        tokens[i] = dml_nikon_relay_register(&table, test_relay_callback, NULL);
        assert(tokens[i] == (1u << i));
    }
    /* 33rd registration fails */
    uint32_t overflow_token = dml_nikon_relay_register(&table, test_relay_callback, NULL);
    assert(overflow_token == 0);
}

static void test_record_translation(void) {
    DmlMenuCommand menu_cmd = 0;
    DmlEvent event = {0};

    /* MENU candidate */
    DmlNikonButtonRecord rec_menu = {.code = DML_NIKON_BTN_CODE_MENU_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_menu, &menu_cmd, &event, 100) == 1);
    assert(menu_cmd == DML_MENU_TOGGLE);
    assert(event.type == DML_EVENT_BUTTON);
    assert(event.button == (int)DML_NIKON_BTN_CODE_MENU_CANDIDATE);
    assert(event.millis == 100);

    /* OK candidate with value_1 == 1 (press) */
    DmlNikonButtonRecord rec_ok_press = {.code = DML_NIKON_BTN_CODE_OK_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_ok_press, &menu_cmd, &event, 200) == 1);
    assert(menu_cmd == DML_MENU_ACTIVATE);

    /* OK candidate with value_1 == 0 (not active press) -> does not trigger menu command */
    DmlNikonButtonRecord rec_ok_idle = {.code = DML_NIKON_BTN_CODE_OK_CANDIDATE, .value_1 = 0};
    assert(dml_nikon_translate_record(&rec_ok_idle, &menu_cmd, &event, 250) == 0);

    /* D-pad navigation */
    DmlNikonButtonRecord rec_up = {.code = DML_NIKON_BTN_CODE_UP_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_up, &menu_cmd, &event, 300) == 1);
    assert(menu_cmd == DML_MENU_PREVIOUS);

    DmlNikonButtonRecord rec_down = {.code = DML_NIKON_BTN_CODE_DOWN_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_down, &menu_cmd, &event, 400) == 1);
    assert(menu_cmd == DML_MENU_NEXT);

    DmlNikonButtonRecord rec_left = {.code = DML_NIKON_BTN_CODE_LEFT_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_left, &menu_cmd, &event, 500) == 1);
    assert(menu_cmd == DML_MENU_PREV_TAB);

    DmlNikonButtonRecord rec_right = {.code = DML_NIKON_BTN_CODE_RIGHT_CANDIDATE, .value_1 = 1};
    assert(dml_nikon_translate_record(&rec_right, &menu_cmd, &event, 600) == 1);
    assert(menu_cmd == DML_MENU_NEXT_TAB);
}

int main(void) {
    test_button_state_rw();
    test_relay_table();
    test_record_translation();
    puts("All Nikon adapter tests passed successfully.");
    return 0;
}

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../payload/dml_payload.h"
#include "../modules/intervalometer.h"

static int g_shutter_count = 0;
static void test_log(void *ctx, const char *msg) {
    (void)ctx;
    (void)msg;
}
static int test_shutter(void *ctx) {
    (void)ctx;
    g_shutter_count++;
    return 1;
}

static void test_payload_lifecycle(void) {
    DmlNikonRelayTable relay_table;
    dml_nikon_relay_init(&relay_table);

    DmlNikonButtonState button_state;
    memset(&button_state, 0, sizeof(button_state));

    DmlDrawBufferTable draw_table;
    dml_draw_buffer_init(&draw_table);
    int buf_idx = dml_draw_buffer_create(&draw_table, 720, 480);
    assert(buf_idx >= 0);
    dml_draw_buffer_select(&draw_table, buf_idx);

    DmlCameraAPI api = {
        .user = NULL,
        .log = test_log,
        .try_shutter = test_shutter,
    };

    DmlPayloadEnv env;
    assert(dml_payload_init(&env, &relay_table, &button_state, &draw_table, &api) == 0);
    assert(env.is_active == 1);
    assert(env.magic == DML_PAYLOAD_MAGIC);
    assert(env.relay_token != 0);

    /* Register intervalometer module into payload runtime */
    DmlIntervalometer intervalometer;
    dml_intervalometer_config(&intervalometer, 1000);
    assert(dml_register(&env.runtime, dml_intervalometer_module(&intervalometer)) == 1);

    /* Register menu item */
    assert(dml_menu_add(&env.menu, (DmlMenuItem){
        .label = "Intervalometer On/Off",
        .state = &intervalometer,
        .activate = dml_intervalometer_toggle,
    }) == 1);

    /* Write MENU button event into button_state */
    DmlNikonButtonRecord rec_menu = {
        .code = DML_NIKON_BTN_CODE_MENU_CANDIDATE,
        .value_1 = 1,
    };
    dml_nikon_button_state_write(&button_state, &rec_menu);
    assert(button_state.ready == 1);

    /* Fire RelayEvent dispatch with matching token */
    dml_nikon_relay_dispatch(&relay_table, env.relay_token);

    /* Menu should now be opened */
    assert(env.menu.visible == 1);
    assert(button_state.ready == 1); /* Read-side seam in firmware does not mutate ready */

    /* Verify DrawBuffer has OSD rendered */
    assert(draw_table.entries[buf_idx].pixels != NULL);
    assert(draw_table.entries[buf_idx].pixels[0] != 0); /* Non-zero background */

    /* Write OK button press event into button_state */
    DmlNikonButtonRecord rec_ok = {
        .code = DML_NIKON_BTN_CODE_OK_CANDIDATE,
        .value_1 = 1,
    };
    dml_nikon_button_state_write(&button_state, &rec_ok);

    /* Dispatch event */
    dml_nikon_relay_dispatch(&relay_table, env.relay_token);

    /* Advance ticks to fire shutter */
    dml_payload_on_tick(&env, 1000);
    dml_payload_on_tick(&env, 2000);
    assert(g_shutter_count >= 1);

    /* Test background task quieting via payload */
    DmlTkTaskCreationPacket bg_pk = {
        .exinf = NULL,
        .tskatr = DML_TK_TA_HLNG,
        .task = NULL,
        .itskpri = 20,
        .stksz = 2048,
        .buf = NULL
    };
    DmlTkId bg_tid = dml_tk_cre_tsk(&bg_pk);
    assert(bg_tid > 0);
    assert(dml_tk_sta_tsk(bg_tid, 0) == DML_TK_E_OK);

    assert(dml_payload_quiet(&env, &bg_tid, 1) == 0);
    assert(env.quiet_state.count == 1);
    assert(dml_payload_unquiet(&env) == 0);
    assert(env.quiet_state.count == 0);

    /* Quiet again before shutdown to verify shutdown unquiets */
    assert(dml_payload_quiet(&env, &bg_tid, 1) == 0);
    assert(env.quiet_state.count == 1);

    /* Shutdown payload */
    assert(dml_payload_shutdown(&env) == 0);
    assert(env.is_active == 0);
    assert(env.relay_token == 0);
    assert(env.quiet_state.count == 0);

    /* Cleanup draw table */
    dml_draw_buffer_cleanup(&draw_table);
}

int main(void) {
    test_payload_lifecycle();
    puts("All D3300-ML payload tests passed successfully.");
    return 0;
}

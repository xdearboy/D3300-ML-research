#include "dml_payload.h"
#include <string.h>

int dml_payload_init(
    DmlPayloadEnv *env,
    DmlNikonRelayTable *relay_table,
    DmlNikonButtonState *button_state,
    DmlDrawBufferTable *draw_table,
    const DmlCameraAPI *api
) {
    if (!env || !relay_table || !button_state || !draw_table || !api) {
        return -1;
    }

    memset(env, 0, sizeof(*env));
    env->magic = DML_PAYLOAD_MAGIC;
    env->version = DML_PAYLOAD_VERSION;
    env->relay_table = relay_table;
    env->button_state = button_state;
    env->draw_table = draw_table;
    env->camera_api = *api;

    dml_init(&env->runtime, &env->camera_api);

    /* Hook into camera's RelayEventEx persistent callback table */
    env->relay_token = dml_nikon_relay_register(
        env->relay_table,
        dml_payload_on_relay_event,
        env
    );
    if (env->relay_token == 0) {
        return -1;
    }

    env->is_active = 1;
    return 0;
}

void dml_payload_on_relay_event(uint32_t event_mask, void *user_context) {
    DmlPayloadEnv *env = (DmlPayloadEnv *)user_context;
    if (!env || !env->is_active || !env->button_state) return;

    (void)event_mask;

    DmlNikonButtonRecord record;
    memset(&record, 0, sizeof(record));

    /* Read pending button event from AppButton handoff seam */
    if (dml_nikon_button_state_read(env->button_state, &record)) {
        DmlMenuCommand command = 0;
        DmlEvent event;
        memset(&event, 0, sizeof(event));

        uint32_t current_millis = 0; /* Could be queried from timer */
        if (dml_nikon_translate_record(&record, &command, &event, current_millis)) {
            if (dml_menu_handle(&env->menu, command, current_millis, &env->camera_api)) {
                dml_payload_render_osd(env);
                return;
            }
        }
        dml_dispatch(&env->runtime, &event);
    }
}

void dml_payload_on_tick(DmlPayloadEnv *env, uint32_t current_millis) {
    if (!env || !env->is_active) return;
    DmlEvent event = {
        .type = DML_EVENT_TICK,
        .millis = current_millis,
        .button = 0,
    };
    dml_dispatch(&env->runtime, &event);
}

void dml_payload_render_osd(DmlPayloadEnv *env) {
    if (!env || !env->is_active || !env->draw_table) return;

    int buf_idx = env->draw_table->current_selected;
    if (buf_idx < 0) {
        buf_idx = 0;
        dml_draw_buffer_select(env->draw_table, buf_idx);
    }

    if (!env->menu.visible) {
        dml_draw_buffer_clear(env->draw_table, buf_idx, 0x00000000u);
        dml_draw_buffer_swap(env->draw_table, buf_idx, 1);
        return;
    }

    /* Render D3300-ML OSD */
    dml_draw_buffer_clear(env->draw_table, buf_idx, 0xCC101010u);
    dml_draw_buffer_fill_rect(env->draw_table, buf_idx, 0, 0, 720, 28, 0xFF1E1E1Eu);
    dml_draw_buffer_draw_text(env->draw_table, buf_idx, 16, 10, "D3300-ML :: ON-CAMERA PAYLOAD", 0xFFFFFFFFu, 0x00000000u);

    for (size_t i = 0; i < env->menu.count; i++) {
        int y = 44 + (int)i * 22;
        if (i == env->menu.selected) {
            dml_draw_buffer_fill_rect(env->draw_table, buf_idx, 8, y - 2, 704, 18, 0xFF0055AAu);
            dml_draw_buffer_draw_text(env->draw_table, buf_idx, 14, y + 3, ">", 0xFFFFFFFFu, 0x00000000u);
            dml_draw_buffer_draw_text(env->draw_table, buf_idx, 28, y + 3, env->menu.items[i].label, 0xFFFFFFFFu, 0x00000000u);
        } else {
            dml_draw_buffer_draw_text(env->draw_table, buf_idx, 28, y + 3, env->menu.items[i].label, 0xFFAAAAAAu, 0x00000000u);
        }
    }
    dml_draw_buffer_swap(env->draw_table, buf_idx, 1);
}

int dml_payload_shutdown(DmlPayloadEnv *env) {
    if (!env || !env->is_active) return -1;

    if (env->relay_token && env->relay_table) {
        dml_nikon_relay_unregister(env->relay_table, env->relay_token);
        env->relay_token = 0;
    }
    env->is_active = 0;
    return 0;
}

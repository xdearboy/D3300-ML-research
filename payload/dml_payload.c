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

    /* Register with the host model of the RelayEventEx callback table. */
    env->relay_token = dml_nikon_relay_register(
        env->relay_table,
        dml_payload_on_relay_event,
        env
    );
    if (env->relay_token == 0) {
        return -1;
    }

    /* Set sensible initial status defaults */
    env->cached_status.shooting_mode = DML_SHOOTING_MODE_MANUAL;
    env->cached_status.shutter_numerator = 1;
    env->cached_status.shutter_denominator = 250;
    env->cached_status.aperture_centi = 280;
    env->cached_status.iso = 400;
    env->cached_status.metering_mode = DML_METERING_MATRIX;
    env->cached_status.af_mode = DML_AF_MODE_AF_S;
    env->cached_status.battery_pct = 84;
    env->cached_status.image_quality = DML_QUALITY_RAW_FINE;
    env->cached_status.shots_remaining = 1420;
    env->cached_status.buffer_remaining = 12;

    env->is_active = 1;
    return 0;
}

void dml_payload_set_hw_status(DmlPayloadEnv *env, const void *raw_ptr) {
    if (!env) return;
    env->hw_status_override = raw_ptr;
}

int dml_payload_toggle_info_screen(DmlPayloadEnv *env) {
    if (!env || !env->is_active) return -1;
    env->info_screen_visible = !env->info_screen_visible;
    return env->info_screen_visible;
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

        uint32_t current_millis = env->current_millis;
        if (record.code == DML_NIKON_BTN_CODE_MENU_CANDIDATE) {
            int is_down = (record.value_1 == 1);
            if (dml_menu_on_menu_button(&env->menu, is_down, current_millis, &command)) {
                if (command == DML_MENU_PASSTHROUGH_FACTORY) {
                    /* Do not consume: pass through to factory Nikon menu */
                    dml_dispatch(&env->runtime, &event);
                    return;
                }
                if (command == DML_MENU_TOGGLE) {
                    dml_menu_handle(&env->menu, command, current_millis, &env->camera_api);
                    dml_payload_render_osd(env);
                    return;
                }
            } else {
                /* Holding in progress: update hold meter */
                dml_payload_render_osd(env);
                return;
            }
        }

        if (dml_nikon_translate_record(&record, &command, &event, current_millis)) {
            if (command == DML_MENU_TOGGLE_INFO) {
                dml_payload_toggle_info_screen(env);
                dml_payload_render_osd(env);
                return;
            }
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
    env->current_millis = current_millis;
    dml_menu_tick(&env->menu, current_millis);
    if (env->menu.is_holding_menu || (env->menu.visible && env->menu.anim_progress < 100)) {
        dml_payload_render_osd(env);
    }
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

    if (env->menu.visible || (env->menu.is_holding_menu && env->menu.hold_progress > 0)) {
        /* Render D3300-ML Flat Cinema Menu OSD or Hold Indicator */
        dml_menu_screen_render(env->draw_table, buf_idx, &env->menu, &env->cached_status);
        dml_draw_buffer_swap(env->draw_table, buf_idx, 1);
        return;
    }

    if (env->info_screen_visible) {
        /* Decode an explicitly supplied simulator fixture if available. */
        DmlCameraStatus status;
        if (dml_camera_status_read_hw(&status, env->hw_status_override) == 0) {
            env->cached_status = status;
        }
        dml_info_screen_render(env->draw_table, buf_idx, &env->cached_status);
        dml_draw_buffer_swap(env->draw_table, buf_idx, 1);
        return;
    }

    /* Neither visible: clear draw buffer */
    dml_draw_buffer_clear(env->draw_table, buf_idx, 0x00000000u);
    dml_draw_buffer_swap(env->draw_table, buf_idx, 1);
}

int dml_payload_quiet(DmlPayloadEnv *env, const DmlTkId *targets, size_t n_targets) {
    if (!env || !env->is_active) return -1;
    return dml_tk_quiet_tasks(&env->quiet_state, targets, n_targets) == DML_TK_E_OK ? 0 : -1;
}

int dml_payload_unquiet(DmlPayloadEnv *env) {
    if (!env || !env->is_active) return -1;
    return dml_tk_unquiet_tasks(&env->quiet_state) == DML_TK_E_OK ? 0 : -1;
}

int dml_payload_shutdown(DmlPayloadEnv *env) {
    if (!env || !env->is_active) return -1;

    if (env->quiet_state.count > 0) {
        dml_tk_unquiet_tasks(&env->quiet_state);
    }

    if (env->relay_token && env->relay_table) {
        dml_nikon_relay_unregister(env->relay_table, env->relay_token);
        env->relay_token = 0;
    }
    env->is_active = 0;
    return 0;
}


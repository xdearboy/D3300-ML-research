#include "dml_nikon_adapter.h"
#include <string.h>

void dml_nikon_button_state_write(DmlNikonButtonState *state, const DmlNikonButtonRecord *record) {
    if (!state || !record) {
        return;
    }
    state->ready = 1u;
    memcpy(&state->record, record, sizeof(DmlNikonButtonRecord));
}

uint32_t dml_nikon_button_state_read(const DmlNikonButtonState *state, DmlNikonButtonRecord *out_record) {
    if (!state) {
        return 0u;
    }
    if (state->ready != 0u && out_record != NULL) {
        memcpy(out_record, &state->record, sizeof(DmlNikonButtonRecord));
    }
    return state->ready;
}

void dml_nikon_relay_init(DmlNikonRelayTable *table) {
    if (!table) {
        return;
    }
    for (uint32_t i = 0u; i < DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS; ++i) {
        table->slots[i].mask = (1u << i);
        table->slots[i].callback = NULL;
        table->slots[i].user_context = NULL;
    }
}

uint32_t dml_nikon_relay_register(DmlNikonRelayTable *table, DmlNikonRelayCallback callback, void *user_context) {
    if (!table || !callback) {
        return 0u;
    }
    for (uint32_t i = 0u; i < DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS; ++i) {
        if (table->slots[i].callback == NULL) {
            table->slots[i].callback = callback;
            table->slots[i].user_context = user_context;
            return table->slots[i].mask;
        }
    }
    return 0u; /* Table full / overflow */
}

int dml_nikon_relay_unregister(DmlNikonRelayTable *table, uint32_t token) {
    if (!table || token == 0u) {
        return 0;
    }
    for (uint32_t i = 0u; i < DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS; ++i) {
        if (table->slots[i].mask == token && table->slots[i].callback != NULL) {
            table->slots[i].callback = NULL;
            table->slots[i].user_context = NULL;
            return 1;
        }
    }
    return 0;
}

void dml_nikon_relay_dispatch(const DmlNikonRelayTable *table, uint32_t event_mask) {
    if (!table || event_mask == 0u) {
        return;
    }
    for (uint32_t i = 0u; i < DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS; ++i) {
        if (table->slots[i].callback != NULL && (table->slots[i].mask & event_mask) != 0u) {
            /* Invokes persistent callback without zeroing slot */
            table->slots[i].callback(event_mask, table->slots[i].user_context);
        }
    }
}

int dml_nikon_translate_record(
    const DmlNikonButtonRecord *record,
    DmlMenuCommand *out_menu_cmd,
    DmlEvent *out_event,
    uint32_t current_millis
) {
    if (!record) {
        return 0;
    }

    if (out_event) {
        out_event->type = DML_EVENT_BUTTON;
        out_event->millis = current_millis;
        out_event->button = (int)record->code;
    }

    if (!out_menu_cmd) {
        return 0;
    }

    switch (record->code) {
        case DML_NIKON_BTN_CODE_MENU_CANDIDATE:
            *out_menu_cmd = DML_MENU_TOGGLE;
            return 1;

        case DML_NIKON_BTN_CODE_OK_CANDIDATE:
            /* Queue button filter requires value_1 == 1 for activation */
            if (record->value_1 == 1u) {
                *out_menu_cmd = DML_MENU_ACTIVATE;
                return 1;
            }
            break;

        case DML_NIKON_BTN_CODE_UP_CANDIDATE:
            *out_menu_cmd = DML_MENU_PREVIOUS;
            return 1;

        case DML_NIKON_BTN_CODE_DOWN_CANDIDATE:
            *out_menu_cmd = DML_MENU_NEXT;
            return 1;

        default:
            break;
    }

    return 0;
}

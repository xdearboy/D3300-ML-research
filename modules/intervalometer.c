#include "intervalometer.h"

static void interval_event(void *ctx, const DmlEvent *event, const DmlCameraAPI *api) {
    DmlIntervalometer *state = (DmlIntervalometer *)ctx;
    if (event->type == DML_EVENT_BUTTON && event->button == 'i') {
        state->active = !state->active;
        if (state->active) state->next_due_ms = event->millis + state->period_ms;
        if (api && api->log)
            api->log(api->user, state->active ? "intervalometer ON" : "intervalometer OFF");
    }
    if (event->type == DML_EVENT_TICK && state->active &&
        (int32_t)(event->millis - state->next_due_ms) >= 0) {
        /* Do not block, retry failed shutter attempts on next tick. */
        if (api && api->try_shutter && api->try_shutter(api->user)) {
            state->shots++;
            state->next_due_ms = event->millis + state->period_ms;
        }
    }
}

static void interval_description(const void *ctx, const DmlCameraAPI *api) {
    const DmlIntervalometer *state = (const DmlIntervalometer *)ctx;
    if (api && api->log) {
        api->log(api->user, state->active ? "intervalometer: running" : "intervalometer: stopped");
    }
}

void dml_intervalometer_config(DmlIntervalometer *state, uint32_t period_ms) {
    state->period_ms = period_ms > 0 && period_ms <= 600000 ? period_ms : 2000;
    state->next_due_ms = 0;
    state->shots = 0;
    state->active = 0;
}

const DmlModule *dml_intervalometer_module(DmlIntervalometer *state) {
    static DmlModule module = {
        DML_ABI_VERSION, "intervalometer", interval_event, interval_description, 0
    };
    module.state = state; /* one-instance host prototype */
    return &module;
}

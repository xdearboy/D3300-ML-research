#ifndef DML_INTERVALOMETER_H
#define DML_INTERVALOMETER_H
#include "../core/dml.h"

typedef struct {
    uint32_t period_ms;
    uint32_t next_due_ms;
    unsigned int shots;
    int active;
    DmlModule module;
} DmlIntervalometer;

void dml_intervalometer_config(DmlIntervalometer *state, uint32_t period_ms);
const DmlModule *dml_intervalometer_module(DmlIntervalometer *state);
void dml_intervalometer_toggle(void *ctx, uint32_t millis, const DmlCameraAPI *api);
#endif

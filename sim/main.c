#include <stdio.h>
#include <stdint.h>
#include "../core/dml.h"
#include "../modules/intervalometer.h"

typedef struct { unsigned int shots; } SimCamera;

static void print_msg(void *user, const char *s) {
    (void)user;
    printf("[module] %s\n", s);
}

static int sim_shutter(void *user) {
    SimCamera *camera = (SimCamera *)user;
    camera->shots++;
    printf("[mock camera] shutter # %u\n", camera->shots);
    return 1;
}

int main(void) {
    SimCamera camera = {0};
    DmlCameraAPI api = {&camera, print_msg, sim_shutter};
    DmlRuntime runtime;
    DmlIntervalometer state;
    uint32_t t;
    dml_init(&runtime, &api);
    dml_intervalometer_config(&state, 2000);
    if (!dml_register(&runtime, dml_intervalometer_module(&state))) return 1;
    puts("D3300 ML host simulator (NOT running on a Nikon camera)");
    dml_describe(&runtime);
    {
        DmlEvent start = {DML_EVENT_BUTTON, 0, 'i'};
        dml_dispatch(&runtime, &start);
    }
    for (t = 0; t <= 10500; t += 500) {
        DmlEvent tick = {DML_EVENT_TICK, t, 0};
        dml_dispatch(&runtime, &tick);
    }
    printf("simulated shots: %u\n", camera.shots);
    return camera.shots == 5 ? 0 : 2;
}

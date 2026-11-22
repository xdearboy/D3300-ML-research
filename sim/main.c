#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
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

static void dispatch(DmlRuntime *runtime, DmlEventType type, uint32_t millis, int key) {
    DmlEvent event = {type, millis, key};
    dml_dispatch(runtime, &event);
}

static void render_menu(const DmlMenu *menu) {
    if (!menu->visible) {
        puts("Menu closed");
        return;
    }
    puts("D3300-ML menu");
    for (size_t i = 0; i < menu->count; ++i) {
        printf("%c %s\n", i == menu->selected ? '>' : ' ', menu->items[i].label);
    }
}

static void describe_modules(void *ctx, uint32_t millis, const DmlCameraAPI *api) {
    (void)millis;
    (void)api;
    dml_describe(ctx);
}

static void handle_key(DmlRuntime *runtime, DmlMenu *menu, uint32_t millis, int key) {
    DmlMenuCommand command = 0;
    switch (key) {
        case 'm': command = DML_MENU_TOGGLE; break;
        case 'w': command = DML_MENU_PREVIOUS; break;
        case 's': command = DML_MENU_NEXT; break;
        case 'e': command = DML_MENU_ACTIVATE; break;
    }
    if (dml_menu_handle(menu, command, millis, runtime->api)) {
        render_menu(menu);
    } else {
        dispatch(runtime, DML_EVENT_BUTTON, millis, key);
    }
}

static void interactive_run(DmlRuntime *runtime, DmlMenu *menu) {
    uint32_t millis = 0;
    int key;
    puts("m menu, w/s select, e activate, t advance 500 ms, q quit");
    while ((key = getchar()) != EOF && key != 'q') {
        if (key == 't') {
            millis += 500;
            dispatch(runtime, DML_EVENT_TICK, millis, 0);
        } else if (key != '\n' && key != '\r') {
            handle_key(runtime, menu, millis, key);
        }
    }
}

int main(int argc, char **argv) {
    int interactive = argc == 2 && strcmp(argv[1], "--interactive") == 0;
    if (argc != 1 && !interactive) {
        fprintf(stderr, "usage: %s [--interactive]\n", argv[0]);
        return 2;
    }
    SimCamera camera = {0};
    DmlCameraAPI api = {&camera, print_msg, sim_shutter};
    DmlRuntime runtime;
    DmlIntervalometer state;
    DmlMenu menu = {0};
    dml_init(&runtime, &api);
    dml_intervalometer_config(&state, 2000);
    if (!dml_register(&runtime, dml_intervalometer_module(&state))) return 1;
    if (!dml_menu_add(&menu, (DmlMenuItem) {"Intervalometer on/off", &state,
                                           dml_intervalometer_toggle}) ||
        !dml_menu_add(&menu, (DmlMenuItem) {"Module status", &runtime, describe_modules})) {
        return 1;
    }
    puts("D3300 ML host simulator (NOT running on a Nikon camera)");
    if (interactive) {
        interactive_run(&runtime, &menu);
    } else {
        dml_describe(&runtime);
        handle_key(&runtime, &menu, 0, 'm');
        handle_key(&runtime, &menu, 0, 'e');
        handle_key(&runtime, &menu, 0, 'm');
        for (uint32_t t = 0; t <= 10500; t += 500) {
            dispatch(&runtime, DML_EVENT_TICK, t, 0);
        }
    }
    printf("simulated shots: %u\n", camera.shots);
    return interactive || camera.shots == 5 ? 0 : 2;
}

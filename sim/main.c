#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
#include "../core/dml_nikon_adapter.h"
#include "../core/dml_draw_buffer.h"
#include "../modules/intervalometer.h"

typedef struct { unsigned int shots; } SimCamera;
static DmlDrawBufferTable g_display_table;
static int g_display_buf_idx = -1;

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
    if (g_display_buf_idx >= 0) {
        if (!menu->visible) {
            /* Black display with subtle LiveView standby text */
            dml_draw_buffer_clear(&g_display_table, g_display_buf_idx, 0xFF000000u);
            dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 16, 16, "LiveView / Standby", 0xFF666666u, 0x00000000u);
        } else {
            /* 720x480 LCD OSD overlay */
            dml_draw_buffer_clear(&g_display_table, g_display_buf_idx, 0xFF141414u);
            /* Header bar */
            dml_draw_buffer_fill_rect(&g_display_table, g_display_buf_idx, 0, 0, 720, 28, 0xFF222222u);
            dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 12, 10, "D3300-ML :: OSD MENU", 0xFFE0E0E0u, 0x00000000u);

            /* Menu items */
            for (size_t i = 0; i < menu->count; ++i) {
                int item_y = 48 + (int)i * 24;
                if (i == menu->selected) {
                    /* Nikon blue highlight for selected entry */
                    dml_draw_buffer_fill_rect(&g_display_table, g_display_buf_idx, 8, item_y - 3, 704, 20, 0xFF004D99u);
                    dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 14, item_y + 3, ">", 0xFFFFFFFFu, 0x00000000u);
                    dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 28, item_y + 3, menu->items[i].label, 0xFFFFFFFFu, 0x00000000u);
                } else {
                    dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 28, item_y + 3, menu->items[i].label, 0xFFA0A0A0u, 0x00000000u);
                }
            }
        }
        dml_draw_buffer_swap(&g_display_table, g_display_buf_idx, 1);
    }

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
    DmlNikonButtonRecord record = {0};
    switch (key) {
        case 'm': record.code = DML_NIKON_BTN_CODE_MENU_CANDIDATE; record.value_1 = 1; break;
        case 'w': record.code = DML_NIKON_BTN_CODE_UP_CANDIDATE; record.value_1 = 1; break;
        case 's': record.code = DML_NIKON_BTN_CODE_DOWN_CANDIDATE; record.value_1 = 1; break;
        case 'e': record.code = DML_NIKON_BTN_CODE_OK_CANDIDATE; record.value_1 = 1; break;
        default:  record.code = (uint16_t)key; record.value_1 = 1; break;
    }

    DmlMenuCommand command = 0;
    DmlEvent event = {0};
    if (dml_nikon_translate_record(&record, &command, &event, millis)) {
        if (dml_menu_handle(menu, command, millis, runtime->api)) {
            render_menu(menu);
            return;
        }
    }
    dml_dispatch(runtime, &event);
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
    dml_draw_buffer_init(&g_display_table);
    g_display_buf_idx = dml_draw_buffer_create(&g_display_table, 720, 480);
    if (g_display_buf_idx >= 0) {
        dml_draw_buffer_select(&g_display_table, g_display_buf_idx);
    }
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
    dml_draw_buffer_cleanup(&g_display_table);
    return interactive || camera.shots == 5 ? 0 : 2;
}

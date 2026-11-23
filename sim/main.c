#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
#include "../core/dml_nikon_adapter.h"
#include "../core/dml_draw_buffer.h"
#include "../core/dml_camera_status.h"
#include "../core/dml_camera_status_hw.h"
#include "../core/dml_info_screen.h"
#include "../core/dml_menu_screen.h"
#include "../modules/intervalometer.h"
#include "../modules/fuji_film_sim.h"

typedef struct { unsigned int shots; } SimCamera;
static DmlDrawBufferTable g_display_table;
static int g_display_buf_idx = -1;
static DmlCameraStatus g_camera_status;
static DmlHwCameraStatusRaw g_hw_raw;
static int g_info_screen_active = 0;

static DmlMenu *g_active_menu_ptr = NULL;

static void sim_intervalometer_toggle(void *ctx, uint32_t millis, const DmlCameraAPI *api) {
    dml_intervalometer_toggle(ctx, millis, api);
    DmlIntervalometer *st = (DmlIntervalometer *)ctx;
    if (g_active_menu_ptr && st) {
        for (size_t i = 0; i < g_active_menu_ptr->count; ++i) {
            if (g_active_menu_ptr->items[i].state == ctx) {
                g_active_menu_ptr->items[i].is_on = st->active ? 1 : 0;
                g_active_menu_ptr->items[i].status_text = st->active ? "ON" : "OFF";
                break;
            }
        }
    }
}

static void print_msg(void *user, const char *s) {
    (void)user;
    printf("[module] %s\n", s);
}

static void dummy_menu_action(void *user, uint32_t millis, const DmlCameraAPI *api) {
    (void)user;
    (void)millis;
    (void)api;
}

static int sim_shutter(void *user) {
    SimCamera *camera = (SimCamera *)user;
    camera->shots++;
    printf("[mock camera] shutter # %u\n", camera->shots);
    if (g_camera_status.shots_remaining > 0) {
        g_camera_status.shots_remaining--;
    }
    return 1;
}

static void dispatch(DmlRuntime *runtime, DmlEventType type, uint32_t millis, int key) {
    DmlEvent event = {type, millis, key};
    dml_dispatch(runtime, &event);
}

static void render_display(const DmlMenu *menu) {
    if (g_display_buf_idx < 0) return;

    if (menu && (menu->visible || (menu->is_holding_menu && menu->hold_progress > 0))) {
        /* Flat Cinema Settings Menu or Hold Progress Meter */
        dml_menu_screen_render(&g_display_table, g_display_buf_idx, menu, &g_camera_status);
    } else if (g_info_screen_active) {
        /* Custom D3300-ML Shooting Information Screen */
        dml_info_screen_render(&g_display_table, g_display_buf_idx, &g_camera_status);
    } else {
        /* Black display with subtle LiveView standby text */
        dml_draw_buffer_clear(&g_display_table, g_display_buf_idx, 0xFF000000u);
        dml_draw_buffer_draw_text(&g_display_table, g_display_buf_idx, 16, 16, "LiveView / Standby", 0xFF666666u, 0x00000000u);
    }
    dml_draw_buffer_swap(&g_display_table, g_display_buf_idx, 1);
}

static void print_terminal_menu(const DmlMenu *menu) {
    if (!menu->visible) {
        puts("Menu closed");
        return;
    }
    printf("=== D3300-ML Cinema OS [%s] ===\n", dml_menu_tab_name((DmlMenuTab)menu->active_tab));
    size_t count = dml_menu_tab_item_count(menu, (DmlMenuTab)menu->active_tab);
    for (size_t i = 0; i < count; ++i) {
        const DmlMenuItem *it = dml_menu_get_tab_item(menu, (DmlMenuTab)menu->active_tab, i);
        if (!it) continue;
        const char *val_str = it->value_text ? it->value_text : (it->status_text ? it->status_text : "");
        printf("%c %-22s  %-10s  %s\n",
               i == menu->selected ? '>' : ' ',
               it->label ? it->label : "",
               val_str,
               it->desc ? it->desc : "");
    }
}

static void describe_modules(void *ctx, uint32_t millis, const DmlCameraAPI *api) {
    (void)millis;
    (void)api;
    dml_describe(ctx);
}

static void action_cycle_font(void *state, uint32_t millis, const DmlCameraAPI *api) {
    (void)millis;
    (void)api;
    DmlMenu *menu = (DmlMenu *)state;
    if (!menu) return;
    dml_menu_cycle_font(menu);
    for (size_t i = 0; i < menu->count; ++i) {
        if (strcmp(menu->items[i].label, "UI FONT") == 0) {
            menu->items[i].value_text = dml_menu_get_font_name(menu->font_id);
            break;
        }
    }
}

static void action_cycle_fuji_profile(void *state, uint32_t millis, const DmlCameraAPI *api) {
    (void)millis;
    (void)api;
    DmlMenu *menu = (DmlMenu *)state;
    if (!menu) return;
    dml_menu_cycle_fuji_profile(menu);
    for (size_t i = 0; i < menu->count; ++i) {
        if (strcmp(menu->items[i].label, "FILM SIMULATION") == 0) {
            menu->items[i].value_text = dml_menu_get_fuji_profile_name(menu->fuji_profile_id);
            break;
        }
    }
    if (menu->fuji_profile_id > 0) {
        const DmlFujiProfile *p = dml_fuji_get_profile((DmlFujiProfileId)(menu->fuji_profile_id - 1));
        dml_fuji_export_to_sd(p, "/tmp", (int)menu->fuji_profile_id);
        printf("[film sim] Active profile: %s -> exported to /tmp/NIKON/CUSTOMPC/PICCON%02d.NOP\n",
               p->name, (int)menu->fuji_profile_id);
    } else {
        puts("[film sim] Profile set to OFF (Stock Nikon picture control)");
    }
}

static void handle_key(DmlRuntime *runtime, DmlMenu *menu, uint32_t millis, int key) {
    if (key == 'i') {
        g_info_screen_active = !g_info_screen_active;
        printf("[display] Custom info screen %s\n", g_info_screen_active ? "OPENED" : "CLOSED");
        render_display(menu);
        return;
    }

    /* Exposure adjustments via raw hardware status memory */
    if (key == '[') {
        if (g_hw_raw.tv_code > 0) g_hw_raw.tv_code--;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char s[32];
        dml_camera_status_format_shutter(&g_camera_status, s, sizeof(s));
        printf("[hw status] Shutter (tv_code=%u): %s\n", g_hw_raw.tv_code, s);
        render_display(menu);
        return;
    }
    if (key == ']') {
        if (g_hw_raw.tv_code < 52) g_hw_raw.tv_code++;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char s[32];
        dml_camera_status_format_shutter(&g_camera_status, s, sizeof(s));
        printf("[hw status] Shutter (tv_code=%u): %s\n", g_hw_raw.tv_code, s);
        render_display(menu);
        return;
    }
    if (key == '-') {
        if (g_hw_raw.av_code > 0) g_hw_raw.av_code--;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char a[32];
        dml_camera_status_format_aperture(&g_camera_status, a, sizeof(a));
        printf("[hw status] Aperture (av_code=%u): %s\n", g_hw_raw.av_code, a);
        render_display(menu);
        return;
    }
    if (key == '=') {
        if (g_hw_raw.av_code < 30) g_hw_raw.av_code++;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char a[32];
        dml_camera_status_format_aperture(&g_camera_status, a, sizeof(a));
        printf("[hw status] Aperture (av_code=%u): %s\n", g_hw_raw.av_code, a);
        render_display(menu);
        return;
    }
    if (key == ',') {
        if ((g_hw_raw.iso_code & 0x7F) > 0) g_hw_raw.iso_code--;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char iso[32];
        dml_camera_status_format_iso(&g_camera_status, iso, sizeof(iso));
        printf("[hw status] ISO (iso_code=%u): %s\n", g_hw_raw.iso_code, iso);
        render_display(menu);
        return;
    }
    if (key == '.') {
        if ((g_hw_raw.iso_code & 0x7F) < 8) g_hw_raw.iso_code++;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        char iso[32];
        dml_camera_status_format_iso(&g_camera_status, iso, sizeof(iso));
        printf("[hw status] ISO (iso_code=%u): %s\n", g_hw_raw.iso_code, iso);
        render_display(menu);
        return;
    }
    if (key == 'f') {
        g_hw_raw.flash_state ^= 0x80;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        printf("[hw status] Flash: %s (%s)\n",
               g_camera_status.flash_raised ? "RAISED" : "LOWERED",
               dml_camera_status_flash_mode_name(g_camera_status.flash_mode));
        render_display(menu);
        return;
    }
    if (key == 'b') {
        /* Cycle battery: 100 -> 84 -> 50 -> 20 -> 5 (critical) */
        uint8_t curr = g_hw_raw.battery_status & 0x7F;
        if (curr >= 100) g_hw_raw.battery_status = 84;
        else if (curr >= 84) g_hw_raw.battery_status = 50;
        else if (curr >= 50) g_hw_raw.battery_status = 20;
        else if (curr >= 20) g_hw_raw.battery_status = 0x85; /* 5% + critical flag */
        else g_hw_raw.battery_status = 100;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        printf("[hw status] Battery: %u%% (warning=%u)\n", g_camera_status.battery_pct, g_camera_status.battery_warning);
        render_display(menu);
        return;
    }
    if (key == 'a') {
        g_hw_raw.active_focus_point = (g_hw_raw.active_focus_point + 1) % 11;
        dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);
        printf("[hw status] Active Focus Point: %u/10\n", g_camera_status.active_focus_point);
        render_display(menu);
        return;
    }

    if (menu && menu->visible) {
        if (key == 'a' || key == 'h' || key == '<') {
            DmlNikonButtonRecord rec = {.code = DML_NIKON_BTN_CODE_LEFT_CANDIDATE, .value_1 = 1};
            DmlMenuCommand cmd = 0;
            DmlEvent ev = {0};
            if (dml_nikon_translate_record(&rec, &cmd, &ev, millis)) {
                dml_menu_handle(menu, cmd, millis, runtime->api);
                render_display(menu);
                print_terminal_menu(menu);
                return;
            }
        }
        if (key == 'd' || key == 'l' || key == '>') {
            DmlNikonButtonRecord rec = {.code = DML_NIKON_BTN_CODE_RIGHT_CANDIDATE, .value_1 = 1};
            DmlMenuCommand cmd = 0;
            DmlEvent ev = {0};
            if (dml_nikon_translate_record(&rec, &cmd, &ev, millis)) {
                dml_menu_handle(menu, cmd, millis, runtime->api);
                render_display(menu);
                print_terminal_menu(menu);
                return;
            }
        }
    }

    if (key == 'H') {
        puts("[menu] Simulating 500ms MENU hold -> Opening D3300-ML with entrance animation");
        DmlMenuCommand cmd = 0;
        dml_menu_on_menu_button(menu, 1, millis, &cmd);
        for (int p = 100; p <= 500; p += 100) {
            dml_menu_tick(menu, millis + p);
            render_display(menu);
        }
        while (menu->anim_progress < 100) {
            dml_menu_tick(menu, millis + 600);
            render_display(menu);
        }
        print_terminal_menu(menu);
        return;
    }

    DmlNikonButtonRecord record = {0};
    switch (key) {
        case 'm': record.code = DML_NIKON_BTN_CODE_MENU_CANDIDATE; record.value_1 = 1; break;
        case 'w': record.code = DML_NIKON_BTN_CODE_UP_CANDIDATE; record.value_1 = 1; break;
        case 's': record.code = DML_NIKON_BTN_CODE_DOWN_CANDIDATE; record.value_1 = 1; break;
        case 'e':
        case '\n':
        case '\r':
            record.code = DML_NIKON_BTN_CODE_OK_CANDIDATE; record.value_1 = 1; break;
        default:  record.code = (uint16_t)key; record.value_1 = 1; break;
    }

    DmlMenuCommand command = 0;
    DmlEvent event = {0};
    if (dml_nikon_translate_record(&record, &command, &event, millis)) {
        if (dml_menu_handle(menu, command, millis, runtime->api)) {
            render_display(menu);
            print_terminal_menu(menu);
            return;
        }
    }
    dml_dispatch(runtime, &event);
}

static void interactive_run(DmlRuntime *runtime, DmlMenu *menu) {
    uint32_t millis = 0;
    int key;
    puts("m menu, H hold menu, i info, w/s nav, a/d tabs, e select, [/] shutter, -/= aperture, ,/. iso, f flash, b battery, t tick, p screenshot, q quit");
    while ((key = getchar()) != EOF && key != 'q') {
        if (key == 't') {
            millis += 500;
            dispatch(runtime, DML_EVENT_TICK, millis, 0);
        } else if (key == 'p') {
            if (g_display_buf_idx >= 0) {
                if (dml_draw_buffer_save_bmp(&g_display_table, g_display_buf_idx, "screenshot.bmp") == 0) {
                    puts("[screenshot] Saved screenshot.bmp");
                } else {
                    puts("[screenshot] Failed to save screenshot.bmp");
                }
            }
        } else if (key != '\n' && key != '\r') {
            handle_key(runtime, menu, millis, key);
        }
    }
}

int main(int argc, char **argv) {
    int interactive = 0;
    const char *screenshot_path = NULL;
    int screenshot_is_info = 0;
    int screenshot_is_hold = 0;
    int initial_tab = -1;
    int font_choice = -1;
    int fuji_choice = -1;

    if (argc == 2 && strcmp(argv[1], "--interactive") == 0) {
        interactive = 1;
    } else if (argc == 3 && strcmp(argv[1], "--screenshot") == 0) {
        screenshot_path = argv[2];
        screenshot_is_info = 0;
    } else if (argc == 3 && strcmp(argv[1], "--screenshot-info") == 0) {
        screenshot_path = argv[2];
        screenshot_is_info = 1;
    } else if (argc == 3 && strcmp(argv[1], "--screenshot-hold") == 0) {
        screenshot_path = argv[2];
        screenshot_is_info = 0;
        screenshot_is_hold = 1;
    } else if (argc == 4 && strcmp(argv[1], "--screenshot-tab") == 0) {
        initial_tab = atoi(argv[2]);
        screenshot_path = argv[3];
        screenshot_is_info = 0;
    } else if (argc == 4 && strcmp(argv[1], "--screenshot-font") == 0) {
        font_choice = atoi(argv[2]);
        screenshot_path = argv[3];
        screenshot_is_info = 0;
    } else if (argc == 4 && strcmp(argv[1], "--screenshot-fuji") == 0) {
        fuji_choice = atoi(argv[2]);
        screenshot_path = argv[3];
        screenshot_is_info = 0;
        initial_tab = 0;
    } else if (argc == 3 && strcmp(argv[1], "--export-fuji-profiles") == 0) {
        int res = dml_fuji_export_all(argv[2]);
        if (res == 0) {
            printf("[fuji_sim] Successfully exported all 5 Fujifilm Picture Control profiles to %s\n", argv[2]);
            return 0;
        } else {
            fprintf(stderr, "[fuji_sim] Failed to export profiles to %s\n", argv[2]);
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--interactive] [--screenshot <path.bmp>] [--screenshot-info <path.bmp>] [--screenshot-hold <path.bmp>] [--screenshot-tab <0..4> <path.bmp>] [--screenshot-font <0..2> <path.bmp>] [--screenshot-fuji <1..5> <path.bmp>] [--export-fuji-profiles <dir>]\n", argv[0]);
        return 2;
    }

    SimCamera camera = {0};
    DmlCameraAPI api = {&camera, print_msg, sim_shutter};
    DmlRuntime runtime;
    DmlIntervalometer state;
    DmlMenu menu = {0};
    g_active_menu_ptr = &menu;

    if (font_choice >= 0 && font_choice < (int)DML_FONT_COUNT) {
        menu.font_id = (uint8_t)font_choice;
    }

    if (fuji_choice >= 0 && fuji_choice <= 5) {
        menu.fuji_profile_id = (uint8_t)fuji_choice;
        menu.selected = 3;
    }

    if (initial_tab >= 0 && initial_tab < (int)DML_TAB_COUNT) {
        menu.active_tab = (size_t)initial_tab;
    }

    memset(&g_hw_raw, 0, sizeof(g_hw_raw));
    g_hw_raw.vtable = D3300_ADDR_CKRRC_VTABLE;
    g_hw_raw.mode_flags = 0x00; /* M mode */
    g_hw_raw.tv_code = 40;      /* 1/250s */
    g_hw_raw.av_code = 9;       /* f/2.8 */
    g_hw_raw.iso_code = 2;      /* ISO 400 */
    g_hw_raw.ev_compensation = 0;
    g_hw_raw.metering_mode = 0; /* Matrix */
    g_hw_raw.af_mode = 0;       /* AF-S */
    g_hw_raw.active_focus_point = 0; /* Center */
    g_hw_raw.release_mode = 0;  /* Single */
    g_hw_raw.flash_state = 0x81;/* Raised + Fill flash */
    g_hw_raw.flash_compensation = 0;
    g_hw_raw.battery_status = 84; /* 84% */
    g_hw_raw.image_quality = 0; /* RAW+Fine */
    g_hw_raw.shots_remaining = 1420;
    g_hw_raw.buffer_remaining = 12;
    g_hw_raw.white_balance = 0; /* Auto */
    dml_camera_status_read_hw(&g_camera_status, &g_hw_raw);

    dml_draw_buffer_init(&g_display_table);
    g_display_buf_idx = dml_draw_buffer_create(&g_display_table, 720, 480);
    if (g_display_buf_idx >= 0) {
        dml_draw_buffer_select(&g_display_table, g_display_buf_idx);
    }
    dml_init(&runtime, &api);
    dml_intervalometer_config(&state, 2000);
    if (!dml_register(&runtime, dml_intervalometer_module(&state))) return 1;

    /* Populate Cinema OS Menu items across tabs */
    /* SHOOT tab */
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "INTERVALOMETER",
        .desc = "Automated timelapse shutter sequence",
        .tab = DML_TAB_SHOOT,
        .value_text = "1.0s",
        .status_text = "OFF",
        .is_on = 0,
        .has_val = 1,
        .state = &state,
        .activate = sim_intervalometer_toggle
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "BURST RATE LIMIT",
        .desc = "Continuous shooting frequency",
        .tab = DML_TAB_SHOOT,
        .value_text = "5.0 FPS",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "SILENT SHUTTER",
        .desc = "Electronic first curtain shutter",
        .tab = DML_TAB_SHOOT,
        .value_text = "",
        .status_text = "OFF",
        .is_on = 0,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "FILM SIMULATION",
        .desc = "Fujifilm Picture Control profile & tone curve",
        .tab = DML_TAB_SHOOT,
        .value_text = dml_menu_get_fuji_profile_name(menu.fuji_profile_id),
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = &menu,
        .activate = action_cycle_fuji_profile
    });

    /* EXPO tab */
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "AUTO-ISO MAXIMUM",
        .desc = "Sensitivity upper threshold",
        .tab = DML_TAB_EXPO,
        .value_text = "6400",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "EXPOSURE OVERRIDE",
        .desc = "Bypass 30s shutter speed limit",
        .tab = DML_TAB_EXPO,
        .value_text = "",
        .status_text = "ON",
        .is_on = 1,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "ZEBRA LEVEL",
        .desc = "Overexposure warning threshold",
        .tab = DML_TAB_EXPO,
        .value_text = "100%",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });

    /* OVERLAY tab */
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "FOCUS PEAKING",
        .desc = "Edge contrast detection outline",
        .tab = DML_TAB_OVERLAY,
        .value_text = "CYAN",
        .status_text = "ON",
        .is_on = 1,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "HISTOGRAM",
        .desc = "Realtime RGB luma distribution",
        .tab = DML_TAB_OVERLAY,
        .value_text = "",
        .status_text = "OFF",
        .is_on = 0,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "CROP MARKS",
        .desc = "Cinema 2.39:1 aspect guidelines",
        .tab = DML_TAB_OVERLAY,
        .value_text = "2.39:1",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });

    /* MOVIE tab (H.264 Full HD video pipeline enhancements) */
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "REMOVE TIME LIMIT",
        .desc = "Bypass factory 20-minute video recording cap",
        .tab = DML_TAB_MOVIE,
        .value_text = "",
        .status_text = "ON",
        .is_on = 1,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "BITRATE OVERRIDE",
        .desc = "H.264 video encoding quality (stock 24 Mbps)",
        .tab = DML_TAB_MOVIE,
        .value_text = "48 Mbps",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "CLEAN HDMI OUT",
        .desc = "Hide HUD overlays on external HDMI recorder",
        .tab = DML_TAB_MOVIE,
        .value_text = "",
        .status_text = "OFF",
        .is_on = 0,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "AUDIO METERS",
        .desc = "Live dual channel audio dB monitor",
        .tab = DML_TAB_MOVIE,
        .value_text = "",
        .status_text = "ON",
        .is_on = 1,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });

    /* SYSTEM tab */
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "UI FONT",
        .desc = "Select interface typography style",
        .tab = DML_TAB_SYSTEM,
        .value_text = dml_menu_get_font_name(menu.font_id),
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = &menu,
        .activate = action_cycle_font
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "TASK MONITOR",
        .desc = "Realtime T-Kernel thread scheduler status",
        .tab = DML_TAB_SYSTEM,
        .value_text = "6 ACT",
        .status_text = "",
        .is_on = 0,
        .has_val = 1,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "SD BENCHMARK",
        .desc = "Card write speed test (/sd0/)",
        .tab = DML_TAB_SYSTEM,
        .value_text = "",
        .status_text = "READY",
        .is_on = 0,
        .has_val = 0,
        .state = NULL,
        .activate = dummy_menu_action
    });
    dml_menu_add(&menu, (DmlMenuItem){
        .label = "MODULE STATUS",
        .desc = "Loaded C modules and memory usage",
        .tab = DML_TAB_SYSTEM,
        .value_text = "",
        .status_text = "OK",
        .is_on = 0,
        .has_val = 0,
        .state = &runtime,
        .activate = describe_modules
    });
    puts("D3300 ML host simulator (NOT running on a Nikon camera)");

    if (interactive) {
        interactive_run(&runtime, &menu);
    } else if (screenshot_path) {
        if (screenshot_is_info) {
            /* Render custom info screen */
            g_info_screen_active = 1;
            render_display(&menu);
        } else if (screenshot_is_hold) {
            /* Render hold progress meter */
            menu.visible = 0;
            menu.is_holding_menu = 1;
            menu.hold_progress = 65;
            render_display(&menu);
        } else {
            /* Open menu so OSD is rendered onto framebuffer */
            menu.anim_progress = 100;
            handle_key(&runtime, &menu, 0, 'm');
        }

        const char *ext = strrchr(screenshot_path, '.');
        int res;
        if (ext && strcmp(ext, ".ppm") == 0) {
            res = dml_draw_buffer_save_ppm(&g_display_table, g_display_buf_idx, screenshot_path);
        } else {
            res = dml_draw_buffer_save_bmp(&g_display_table, g_display_buf_idx, screenshot_path);
        }
        if (res == 0) {
            printf("[screenshot] Exported %s to %s\n", screenshot_is_info ? "info screen" : "OSD menu", screenshot_path);
        } else {
            fprintf(stderr, "[screenshot] Failed to export to %s\n", screenshot_path);
            dml_draw_buffer_cleanup(&g_display_table);
            return 1;
        }
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
    return interactive || screenshot_path || camera.shots == 5 ? 0 : 2;
}

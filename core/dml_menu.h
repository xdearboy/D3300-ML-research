#ifndef DML_MENU_H
#define DML_MENU_H

#include "dml.h"
#include <stddef.h>
#include <stdint.h>

#define DML_MENU_MAX_ITEMS 32u

typedef enum {
    DML_MENU_TOGGLE = 1,
    DML_MENU_PREVIOUS,
    DML_MENU_NEXT,
    DML_MENU_ACTIVATE,
    DML_MENU_TOGGLE_INFO,
    DML_MENU_PREV_TAB,
    DML_MENU_NEXT_TAB,
    DML_MENU_PASSTHROUGH_FACTORY
} DmlMenuCommand;

typedef enum {
    DML_TAB_SHOOT = 0,
    DML_TAB_EXPO = 1,
    DML_TAB_OVERLAY = 2,
    DML_TAB_MOVIE = 3,
    DML_TAB_SYSTEM = 4,
    DML_TAB_COUNT = 5
} DmlMenuTab;

typedef struct {
    const char *label;
    void *state;
    void (*activate)(void *state, uint32_t millis, const DmlCameraAPI *api);
    const char *desc;
    DmlMenuTab tab;
    const char *value_text;
    const char *status_text;
    uint8_t is_on;
    uint8_t has_val;
} DmlMenuItem;

/* Zero-initialize before use. The caller owns the item labels and state. */
typedef struct {
    DmlMenuItem items[DML_MENU_MAX_ITEMS];
    size_t count;
    size_t selected; /* selected index among visible items on active tab */
    size_t active_tab;
    int visible;

    /* Long-press & entrance animation state */
    uint32_t hold_start_ms;
    uint8_t is_holding_menu;
    uint8_t hold_progress; /* 0..100 (%) */
    uint8_t anim_progress; /* 0..100 (%) */

    /* Typography setting (defaults to DML_FONT_MODERN_SANS) */
    uint8_t font_id;

    /* Film Simulation profile: 0 = OFF, 1..5 = Fuji profiles */
    uint8_t fuji_profile_id;
} DmlMenu;

int dml_menu_add(DmlMenu *menu, DmlMenuItem item);
int dml_menu_handle(DmlMenu *menu, DmlMenuCommand command, uint32_t millis,
                    const DmlCameraAPI *api);
void dml_menu_tick(DmlMenu *menu, uint32_t millis);
int dml_menu_on_menu_button(DmlMenu *menu, int is_down, uint32_t millis, DmlMenuCommand *out_cmd);
const char *dml_menu_tab_name(DmlMenuTab tab);
size_t dml_menu_tab_item_count(const DmlMenu *menu, DmlMenuTab tab);
const DmlMenuItem *dml_menu_get_tab_item(const DmlMenu *menu, DmlMenuTab tab, size_t index);
void dml_menu_cycle_font(DmlMenu *menu);
const char *dml_menu_get_font_name(uint8_t font_id);
void dml_menu_cycle_fuji_profile(DmlMenu *menu);
const char *dml_menu_get_fuji_profile_name(uint8_t id);

#endif /* DML_MENU_H */

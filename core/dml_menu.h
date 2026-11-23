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
    DML_MENU_NEXT_TAB
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
} DmlMenu;

int dml_menu_add(DmlMenu *menu, DmlMenuItem item);
int dml_menu_handle(DmlMenu *menu, DmlMenuCommand command, uint32_t millis,
                    const DmlCameraAPI *api);
const char *dml_menu_tab_name(DmlMenuTab tab);
size_t dml_menu_tab_item_count(const DmlMenu *menu, DmlMenuTab tab);
const DmlMenuItem *dml_menu_get_tab_item(const DmlMenu *menu, DmlMenuTab tab, size_t index);

#endif /* DML_MENU_H */

#include "dml_menu.h"
#include <string.h>

const char *dml_menu_tab_name(DmlMenuTab tab) {
    switch (tab) {
        case DML_TAB_SHOOT:   return "SHOOT";
        case DML_TAB_EXPO:    return "EXPO";
        case DML_TAB_OVERLAY: return "OVERLAY";
        case DML_TAB_MOVIE:   return "MOVIE";
        case DML_TAB_SYSTEM:  return "SYSTEM";
        default:              return "MENU";
    }
}

size_t dml_menu_tab_item_count(const DmlMenu *menu, DmlMenuTab tab) {
    if (!menu) return 0;
    size_t count = 0;
    for (size_t i = 0; i < menu->count; ++i) {
        if (menu->items[i].tab == tab) {
            count++;
        }
    }
    return count;
}

const DmlMenuItem *dml_menu_get_tab_item(const DmlMenu *menu, DmlMenuTab tab, size_t index) {
    if (!menu) return NULL;
    size_t current = 0;
    for (size_t i = 0; i < menu->count; ++i) {
        if (menu->items[i].tab == tab) {
            if (current == index) {
                return &menu->items[i];
            }
            current++;
        }
    }
    return NULL;
}

static DmlMenuItem *get_tab_item_mut(DmlMenu *menu, DmlMenuTab tab, size_t index) {
    if (!menu) return NULL;
    size_t current = 0;
    for (size_t i = 0; i < menu->count; ++i) {
        if (menu->items[i].tab == tab) {
            if (current == index) {
                return &menu->items[i];
            }
            current++;
        }
    }
    return NULL;
}

int dml_menu_add(DmlMenu *menu, DmlMenuItem item) {
    if (!item.label || !item.activate || menu->count >= DML_MENU_MAX_ITEMS) {
        return 0;
    }
    menu->items[menu->count++] = item;
    return 1;
}

int dml_menu_handle(DmlMenu *menu, DmlMenuCommand command, uint32_t millis,
                    const DmlCameraAPI *api) {
    if (!menu) return 0;

    if (command == DML_MENU_TOGGLE) {
        menu->visible = !menu->visible;
        return 1;
    }
    if (!menu->visible) {
        return 0;
    }

    size_t tab_count = dml_menu_tab_item_count(menu, (DmlMenuTab)menu->active_tab);
    size_t effective_count = (tab_count > 0) ? tab_count : menu->count;

    switch (command) {
        case DML_MENU_PREV_TAB:
            menu->active_tab = (menu->active_tab + DML_TAB_COUNT - 1) % DML_TAB_COUNT;
            menu->selected = 0;
            return 1;

        case DML_MENU_NEXT_TAB:
            menu->active_tab = (menu->active_tab + 1) % DML_TAB_COUNT;
            menu->selected = 0;
            return 1;

        case DML_MENU_PREVIOUS:
            if (effective_count) {
                menu->selected = (menu->selected + effective_count - 1) % effective_count;
            }
            return 1;

        case DML_MENU_NEXT:
            if (effective_count) {
                menu->selected = (menu->selected + 1) % effective_count;
            }
            return 1;

        case DML_MENU_ACTIVATE:
            if (effective_count) {
                DmlMenuItem *item = NULL;
                if (tab_count > 0) {
                    item = get_tab_item_mut(menu, (DmlMenuTab)menu->active_tab, menu->selected);
                } else if (menu->selected < menu->count) {
                    item = &menu->items[menu->selected];
                }
                if (item && item->activate) {
                    item->activate(item->state, millis, api);
                }
            }
            return 1;

        default:
            return 0;
    }
}

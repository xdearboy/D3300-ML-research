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
        if (menu->visible) {
            menu->anim_progress = 0;
            menu->hold_progress = 0;
            menu->is_holding_menu = 0;
        }
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

int dml_menu_on_menu_button(DmlMenu *menu, int is_down, uint32_t millis, DmlMenuCommand *out_cmd) {
    if (!menu || !out_cmd) return 0;

    if (menu->visible) {
        if (is_down) {
            *out_cmd = DML_MENU_TOGGLE;
            return 1;
        }
        return 0;
    }

    if (is_down) {
        menu->is_holding_menu = 1;
        menu->hold_start_ms = millis;
        menu->hold_progress = 0;
        return 0;
    } else {
        if (menu->is_holding_menu) {
            menu->is_holding_menu = 0;
            if (menu->hold_progress < 100) {
                /* Short tap (<500ms): Pass through to factory Nikon menu */
                menu->hold_progress = 0;
                *out_cmd = DML_MENU_PASSTHROUGH_FACTORY;
                return 1;
            }
            menu->hold_progress = 0;
        }
    }
    return 0;
}

void dml_menu_tick(DmlMenu *menu, uint32_t millis) {
    if (!menu) return;

    if (menu->is_holding_menu && !menu->visible) {
        uint32_t elapsed = (millis >= menu->hold_start_ms) ? (millis - menu->hold_start_ms) : 0;
        if (elapsed >= 500) {
            menu->hold_progress = 100;
            menu->is_holding_menu = 0;
            menu->visible = 1;
            menu->anim_progress = 0;
            return;
        } else {
            menu->hold_progress = (uint8_t)((elapsed * 100) / 500);
        }
    }

    if (menu->visible && menu->anim_progress < 100) {
        if (menu->anim_progress + 25 >= 100) {
            menu->anim_progress = 100;
        } else {
            menu->anim_progress += 25;
        }
    }
}

void dml_menu_cycle_font(DmlMenu *menu) {
    if (!menu) return;
    menu->font_id = (uint8_t)((menu->font_id + 1) % 3);
}

const char *dml_menu_get_font_name(uint8_t font_id) {
    switch (font_id) {
        case 0: return "MODERN SANS";
        case 1: return "MONO TECH";
        case 2: return "RETRO MATRIX";
        default: return "MODERN SANS";
    }
}

void dml_menu_cycle_fuji_profile(DmlMenu *menu) {
    if (!menu) return;
    menu->fuji_profile_id = (uint8_t)((menu->fuji_profile_id + 1) % 6);
}

const char *dml_menu_get_fuji_profile_name(uint8_t id) {
    switch (id) {
        case 0: return "OFF";
        case 1: return "CLASSIC CHROME";
        case 2: return "FUJI VELVIA";
        case 3: return "FUJI ACROS";
        case 4: return "CLASSIC NEG";
        case 5: return "FUJI ASTIA";
        default: return "OFF";
    }
}

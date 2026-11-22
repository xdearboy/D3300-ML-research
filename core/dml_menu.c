#include "dml_menu.h"

int dml_menu_add(DmlMenu *menu, DmlMenuItem item) {
    if (!item.label || !item.activate || menu->count >= DML_MENU_MAX_ITEMS) {
        return 0;
    }
    menu->items[menu->count++] = item;
    return 1;
}

int dml_menu_handle(DmlMenu *menu, DmlMenuCommand command, uint32_t millis,
                    const DmlCameraAPI *api) {
    if (command == DML_MENU_TOGGLE) {
        menu->visible = !menu->visible;
        return 1;
    }
    if (!menu->visible) {
        return 0;
    }
    switch (command) {
        case DML_MENU_PREVIOUS:
            if (menu->count) {
                menu->selected = (menu->selected + menu->count - 1) % menu->count;
            }
            return 1;
        case DML_MENU_NEXT:
            if (menu->count) {
                menu->selected = (menu->selected + 1) % menu->count;
            }
            return 1;
        case DML_MENU_ACTIVATE:
            if (menu->count) {
                const DmlMenuItem *item = &menu->items[menu->selected];
                item->activate(item->state, millis, api);
            }
            return 1;
        default:
            return 0;
    }
}

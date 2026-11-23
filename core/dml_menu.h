#ifndef DML_MENU_H
#define DML_MENU_H

#include "dml.h"

#define DML_MENU_MAX_ITEMS 8u

typedef enum {
    DML_MENU_TOGGLE = 1,
    DML_MENU_PREVIOUS,
    DML_MENU_NEXT,
    DML_MENU_ACTIVATE,
    DML_MENU_TOGGLE_INFO
} DmlMenuCommand;

typedef struct {
    const char *label;
    void *state;
    void (*activate)(void *state, uint32_t millis, const DmlCameraAPI *api);
} DmlMenuItem;

/* Zero-initialize before use. The caller owns the item labels and state. */
typedef struct {
    DmlMenuItem items[DML_MENU_MAX_ITEMS];
    size_t count;
    size_t selected;
    int visible;
} DmlMenu;

int dml_menu_add(DmlMenu *menu, DmlMenuItem item);
/* Returns 1 if the menu consumed the command. Call on the event loop. */
int dml_menu_handle(DmlMenu *menu, DmlMenuCommand command, uint32_t millis,
                    const DmlCameraAPI *api);

#endif

#ifndef DML_UI_LOGICAL_EVENT_H
#define DML_UI_LOGICAL_EVENT_H

#include <stdint.h>

/*
 * Host-side catalogue of the logical IDs dispatched by CMENUTopMenuView in
 * D3300 C 1.02.  It is intentionally not a camera-input adapter: no physical
 * button, GPIO pin, RAM callback, or firmware-injection relationship follows
 * from these values.  See reports/verified-menu-architecture.md.
 */

typedef struct {
    uint8_t logical_id;
    uint16_t vtable_slot_offset;
    uint32_t rom_target;
} DmlUiLogicalRoute;

/*
 * Looks up routes proven by the CMENUTopMenuView TBB at 0x90254E18.
 * Returns 1 when the logical ID has a recorded route, otherwise 0.
 */
int dml_ui_logical_route_lookup(uint8_t logical_id, DmlUiLogicalRoute *out_route);

#endif /* DML_UI_LOGICAL_EVENT_H */

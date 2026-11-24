#include "dml_ui_logical_event.h"

typedef struct {
    uint8_t logical_id;
    uint16_t slot;
    uint32_t target;
} DmlUiLogicalRouteEntry;

/*
 * Addresses and slots are static-analysis facts for the audited C 1.02 ROM.
 * Semantic names such as "confirm" or "back" are deliberately omitted:
 * their UI roles remain inference rather than a physical-input contract.
 */
static const DmlUiLogicalRouteEntry k_routes[] = {
    { 0x01u, 0x12Cu, 0x90076FF4u },
    { 0x05u, 0x130u, 0x90077512u },
    { 0x06u, 0x134u, 0x90077540u },
    { 0x07u, 0x138u, 0x90077548u },
    { 0x08u, 0x13Cu, 0x90077550u },
    { 0x1Bu, 0x17Cu, 0x90077CBDu },
};

int dml_ui_logical_route_lookup(uint8_t logical_id, DmlUiLogicalRoute *out_route) {
    if (!out_route) {
        return 0;
    }
    for (unsigned int i = 0; i < sizeof(k_routes) / sizeof(k_routes[0]); ++i) {
        if (k_routes[i].logical_id == logical_id) {
            out_route->logical_id = k_routes[i].logical_id;
            out_route->vtable_slot_offset = k_routes[i].slot;
            out_route->rom_target = k_routes[i].target;
            return 1;
        }
    }
    return 0;
}

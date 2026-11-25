#ifndef DML_NIKON_MENU_CONTRACT_H
#define DML_NIKON_MENU_CONTRACT_H

#include <stdint.h>

#include "dml_nikon_adapter.h"

/*
 * Host-only representation of the verified TBB at 0x90252DAA in D3300 C 1.02.
 * It translates a normalized queue-record code (0..58) to a logical UI ID.
 * Neither side of this contract identifies a physical camera control, a GPIO pin,
 * or a safe firmware integration point.
 */
typedef struct {
    uint16_t raw_code;
    uint8_t logical_id;
} DmlNikonMenuLogicalEvent;

/*
 * Translate record->code using the 59-entry ROM table at 0x90252DAA.
 * Returns 1 for a mapped logical ID and 0 for an unmapped/out-of-range code.
 * `value_1` and all physical-control semantics are intentionally ignored.
 */
int dml_nikon_menu_logical_event(
    const DmlNikonButtonRecord *record,
    DmlNikonMenuLogicalEvent *out_event
);

#endif /* DML_NIKON_MENU_CONTRACT_H */

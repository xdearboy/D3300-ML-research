#include "dml_nikon_menu_contract.h"

/* Raw queue code -> logical menu ID, copied from the audited 59-byte TBB.
 * Zero represents the ROM default/no-logical-event path. */
static const uint8_t k_raw_to_logical[59] = {
    23u, 13u, 15u, 16u, 17u, 18u, 19u, 20u, 0u, 0u,
    21u, 27u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
    9u, 10u, 11u, 12u, 26u, 25u, 28u, 0u, 33u, 34u,
    35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u,
    14u, 0u, 0u, 22u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 24u,
};

int dml_nikon_menu_logical_event(
    const DmlNikonButtonRecord *record,
    DmlNikonMenuLogicalEvent *out_event
) {
    uint8_t logical_id;

    if (!record || !out_event || record->code >= 59u) {
        return 0;
    }
    logical_id = k_raw_to_logical[record->code];
    if (logical_id == 0u) {
        return 0;
    }
    out_event->raw_code = record->code;
    out_event->logical_id = logical_id;
    return 1;
}

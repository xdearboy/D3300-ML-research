#ifndef DML_NIKON_ADAPTER_H
#define DML_NIKON_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include "dml.h"
#include "dml_menu.h"

/* Verified Nikon D3300 C 1.02 wire structures and integration seams.
   Directly models verified firmware ABIs documented in reports/verified-event-button-paths.json
   and reports/D3300_C102_input_gui.md. */

#define DML_NIKON_QUEUE_RECORD_SIZE 16u
#define DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS 32u

/* Candidate button codes verified in B firmware input pipeline */
#define DML_NIKON_BTN_CODE_MENU_CANDIDATE 0x000Bu
#define DML_NIKON_BTN_CODE_OK_CANDIDATE   0x000Cu
#define DML_NIKON_BTN_CODE_UP_CANDIDATE   0x0048u
#define DML_NIKON_BTN_CODE_DOWN_CANDIDATE 0x004Au

/* 16-byte input queue record format established at COPNOProductManager / AppButton */
typedef struct {
    uint16_t code;        /* Event / object code, or 0xFFFF */
    uint16_t object_id;   /* Target object ID from object+0x12 */
    uint32_t value_1;     /* First data word (e.g. 1 on press/active) */
    uint32_t value_2;     /* Second data word */
    uint32_t queue_mask;  /* Bitmask snapshot from queue+0x10 */
} DmlNikonButtonRecord;

/* State container used by AppButton handoff at 0x903512A0 (write) and 0x9035128C (read) */
typedef struct {
    uint8_t pad_0x00[0x10];
    uint32_t ready;       /* offset 0x10: 1 when new record is stored, 0 otherwise */
    uint32_t status_flags;/* offset 0x14: secondary status */
    uint8_t pad_0x18[0x18];
    DmlNikonButtonRecord record; /* offset 0x30..0x3C: four words (16 bytes) */
    uint8_t extra_byte;   /* offset 0x40 */
} DmlNikonButtonState;

/* Verified 0x903512A0 writer: R0=state, R1=16-byte record.
   Sets ready flag at +0x10 to 1 and copies 16 bytes to +0x30..+0x3C. */
void dml_nikon_button_state_write(DmlNikonButtonState *state, const DmlNikonButtonRecord *record);

/* Verified 0x9035128C reader: R0=state, R1=destination buffer (or NULL for polling).
   If ready is non-zero and out_record != NULL, copies 16 bytes. Returns ready flag. */
uint32_t dml_nikon_button_state_read(const DmlNikonButtonState *state, DmlNikonButtonRecord *out_record);

/* RelayEventEx callback type: persistent callback invoked when event bitmask matches */
typedef void (*DmlNikonRelayCallback)(uint32_t event_mask, void *user_context);

typedef struct {
    uint32_t mask;
    DmlNikonRelayCallback callback;
    void *user_context;
} DmlNikonRelaySlot;

typedef struct {
    DmlNikonRelaySlot slots[DML_NIKON_RELAY_EVENT_EX_MAX_SLOTS];
} DmlNikonRelayTable;

/* Initializes table with default masks (1 << i) and NULL callbacks */
void dml_nikon_relay_init(DmlNikonRelayTable *table);

/* Verified 0x900272A6 registration API: returns token (1 << slot) or 0 on overflow */
uint32_t dml_nikon_relay_register(DmlNikonRelayTable *table, DmlNikonRelayCallback callback, void *user_context);

/* Verified 0x90027316 unregistration API */
int dml_nikon_relay_unregister(DmlNikonRelayTable *table, uint32_t token);

/* Verified 0x900271A4 worker dispatch: tests (mask & event_mask) and invokes callback without clearing slot */
void dml_nikon_relay_dispatch(const DmlNikonRelayTable *table, uint32_t event_mask);

/* Translates 16-byte record into high-level menu commands or camera events */
int dml_nikon_translate_record(
    const DmlNikonButtonRecord *record,
    DmlMenuCommand *out_menu_cmd,
    DmlEvent *out_event,
    uint32_t current_millis
);

#endif /* DML_NIKON_ADAPTER_H */

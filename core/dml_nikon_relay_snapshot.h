#ifndef DML_NIKON_RELAY_SNAPSHOT_H
#define DML_NIKON_RELAY_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

/* Host-only parser for a read-only RAM snapshot. It never writes to the
   supplied memory and does not claim that live camera memory is accessible. */
#define DML_NIKON_RELAY_SNAPSHOT_TABLE 0x10121EB4u
#define DML_NIKON_RELAY_SNAPSHOT_SLOTS 32u
#define DML_NIKON_RELAY_SNAPSHOT_SLOT_SIZE 8u

typedef struct {
    uint32_t event_mask;
    uint32_t callback_address;
    uint8_t active;
} DmlNikonRelaySnapshotSlot;

typedef struct {
    uint32_t table_address;
    uint32_t slot_count;
    DmlNikonRelaySnapshotSlot slots[DML_NIKON_RELAY_SNAPSHOT_SLOTS];
} DmlNikonRelaySnapshot;

/* Parse the verified 32-slot table from a caller-provided RAM snapshot. */
int dml_nikon_relay_snapshot_read(const uint8_t *ram, size_t ram_size,
                                  uint32_t ram_base,
                                  DmlNikonRelaySnapshot *out);

size_t dml_nikon_relay_snapshot_active_count(const DmlNikonRelaySnapshot *snapshot);

#endif /* DML_NIKON_RELAY_SNAPSHOT_H */

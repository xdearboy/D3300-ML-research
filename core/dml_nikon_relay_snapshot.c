#include "dml_nikon_relay_snapshot.h"

static uint32_t read_le32(const uint8_t *p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int dml_nikon_relay_snapshot_read(const uint8_t *ram, size_t ram_size,
                                  uint32_t ram_base,
                                  DmlNikonRelaySnapshot *out) {
    const uint32_t table = DML_NIKON_RELAY_SNAPSHOT_TABLE;
    const size_t bytes = DML_NIKON_RELAY_SNAPSHOT_SLOTS *
                         DML_NIKON_RELAY_SNAPSHOT_SLOT_SIZE;
    size_t offset;

    if (!ram || !out || table < ram_base) {
        return -1;
    }
    offset = (size_t)(table - ram_base);
    if (offset > ram_size || bytes > ram_size - offset) {
        return -1;
    }

    out->table_address = table;
    out->slot_count = DML_NIKON_RELAY_SNAPSHOT_SLOTS;
    for (size_t i = 0u; i < DML_NIKON_RELAY_SNAPSHOT_SLOTS; ++i) {
        const uint8_t *slot = ram + offset + i * DML_NIKON_RELAY_SNAPSHOT_SLOT_SIZE;
        out->slots[i].event_mask = read_le32(slot);
        out->slots[i].callback_address = read_le32(slot + 4u);
        out->slots[i].active = (out->slots[i].callback_address != 0u) ? 1u : 0u;
    }
    return 0;
}

size_t dml_nikon_relay_snapshot_active_count(const DmlNikonRelaySnapshot *snapshot) {
    size_t count = 0u;
    if (!snapshot) {
        return 0u;
    }
    for (size_t i = 0u; i < DML_NIKON_RELAY_SNAPSHOT_SLOTS; ++i) {
        count += snapshot->slots[i].active != 0u;
    }
    return count;
}

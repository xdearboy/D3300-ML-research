#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../core/dml_nikon_relay_snapshot.h"

static void put_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

int main(void) {
    uint8_t ram[0x120] = {0};
    DmlNikonRelaySnapshot snapshot;
    uint8_t before[sizeof(ram)];
    const size_t table_offset = 0x20u;

    put_le32(ram + table_offset + 0x00u, 0x00000001u);
    put_le32(ram + table_offset + 0x04u, 0x9002758Bu);
    put_le32(ram + table_offset + 0x08u, 0x00000002u);
    put_le32(ram + table_offset + 0x0Cu, 0u);
    memcpy(before, ram, sizeof(ram));

    assert(dml_nikon_relay_snapshot_read(ram, sizeof(ram),
                                         DML_NIKON_RELAY_SNAPSHOT_TABLE - (uint32_t)table_offset,
                                         &snapshot) == 0);
    assert(snapshot.table_address == DML_NIKON_RELAY_SNAPSHOT_TABLE);
    assert(snapshot.slot_count == 32u);
    assert(snapshot.slots[0].event_mask == 1u);
    assert(snapshot.slots[0].callback_address == 0x9002758Bu);
    assert(snapshot.slots[0].active == 1u);
    assert(snapshot.slots[1].event_mask == 2u);
    assert(snapshot.slots[1].active == 0u);
    assert(dml_nikon_relay_snapshot_active_count(&snapshot) == 1u);
    assert(memcmp(before, ram, sizeof(ram)) == 0);

    assert(dml_nikon_relay_snapshot_read(ram, 0x20u, 0x10100000u, &snapshot) == -1);
    assert(dml_nikon_relay_snapshot_read(NULL, sizeof(ram), 0x10100000u, &snapshot) == -1);
    return 0;
}

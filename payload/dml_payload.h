#ifndef DML_PAYLOAD_H
#define DML_PAYLOAD_H

#include <stdint.h>
#include <stddef.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
#include "../core/dml_nikon_adapter.h"
#include "../core/dml_draw_buffer.h"
#include "../core/dml_tkernel.h"

/* Magic signature for D3300-ML relocatable payload */
#define DML_PAYLOAD_MAGIC   0x4433334Du /* "D33M" */
#define DML_PAYLOAD_VERSION 0x00010000u /* 1.0.0 */

/* Verified target addresses for Nikon D3300 C 1.02 B-firmware */
#define D3300_C102_ADDR_RELAY_REGISTER   0x900272A6u
#define D3300_C102_ADDR_RELAY_UNREGISTER 0x90027316u
#define D3300_C102_ADDR_BUTTON_READ      0x9035128Cu
#define D3300_C102_ADDR_DRAW_TABLE       0x104A0868u
#define D3300_C102_ADDR_TK_SND_MBF       0x9002EC0Eu

/* Payload runtime environment */
typedef struct {
    uint32_t magic;
    uint32_t version;
    int is_active;
    uint32_t relay_token;

    /* Hardware / firmware pointers */
    DmlNikonRelayTable  *relay_table;
    DmlNikonButtonState *button_state;
    DmlDrawBufferTable  *draw_table;

    /* DML engine components */
    DmlRuntime   runtime;
    DmlMenu      menu;
    DmlCameraAPI camera_api;
} DmlPayloadEnv;

/* Initializes payload, binds to firmware addresses or mock environment */
int dml_payload_init(
    DmlPayloadEnv *env,
    DmlNikonRelayTable *relay_table,
    DmlNikonButtonState *button_state,
    DmlDrawBufferTable *draw_table,
    const DmlCameraAPI *api
);

/* Main event callback hooked into RelayEventEx */
void dml_payload_on_relay_event(uint32_t event_mask, void *user_context);

/* Tick handler called periodically or on vsync */
void dml_payload_on_tick(DmlPayloadEnv *env, uint32_t current_millis);

/* Renders D3300-ML OSD overlay onto camera's active draw buffer */
void dml_payload_render_osd(DmlPayloadEnv *env);

/* Shuts down payload and unhooks callbacks */
int dml_payload_shutdown(DmlPayloadEnv *env);

#endif /* DML_PAYLOAD_H */

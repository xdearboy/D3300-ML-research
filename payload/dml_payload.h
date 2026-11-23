#ifndef DML_PAYLOAD_H
#define DML_PAYLOAD_H

#include <stdint.h>
#include <stddef.h>
#include "../core/dml.h"
#include "../core/dml_menu.h"
#include "../core/dml_nikon_adapter.h"
#include "../core/dml_draw_buffer.h"
#include "../core/dml_tkernel.h"
#include "../core/dml_camera_status.h"
#include "../core/dml_camera_status_hw.h"
#include "../core/dml_info_screen.h"
#include "../core/dml_menu_screen.h"

/* Magic signature for the host-side payload lifecycle prototype. */
#define DML_PAYLOAD_MAGIC   0x4433334Du /* "D33M" */
#define DML_PAYLOAD_VERSION 0x00010000u /* 1.0.0 */

/* Research anchors only. They are not a verified injection or runtime ABI. */
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
    DmlRuntime      runtime;
    DmlMenu         menu;
    DmlCameraAPI    camera_api;
    DmlTkQuietState quiet_state;

    /* Cinema Info Monitor state */
    int             info_screen_visible;
    const void     *hw_status_override; /* If non-NULL, used as raw CKRRCCameraStatus pointer */
    uint32_t        current_millis;      /* Simulator clock updated by dml_payload_on_tick */
    DmlCameraStatus cached_status;      /* Last read/rendered status */
} DmlPayloadEnv;

/* Sets raw hardware pointer override for camera status reading (useful in tests/mocks) */
void dml_payload_set_hw_status(DmlPayloadEnv *env, const void *raw_ptr);

/* Toggles the Cinema Monitor display on or off */
int dml_payload_toggle_info_screen(DmlPayloadEnv *env);

/* Initializes the host-side lifecycle model using caller-provided mocks. */
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

/* Renders into the host DrawBuffer model. Camera integration is not implemented. */
void dml_payload_render_osd(DmlPayloadEnv *env);

/* Exercises simulated T-Kernel task suspension; no camera task IDs are verified. */
int dml_payload_quiet(DmlPayloadEnv *env, const DmlTkId *targets, size_t n_targets);

/* Resumes background tasks when OSD is closed */
int dml_payload_unquiet(DmlPayloadEnv *env);

/* Shuts down payload and unhooks callbacks */
int dml_payload_shutdown(DmlPayloadEnv *env);

#endif /* DML_PAYLOAD_H */


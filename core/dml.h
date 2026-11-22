#ifndef DML_H
#define DML_H
#include <stddef.h>
#include <stdint.h>

/* D3300 Magic-Lantern-like extension core: HOST PROTOTYPE ONLY.
   No addresses, hooks, Nikon ABIs, or real device support are assumed. */
#define DML_ABI_VERSION 1u
#define DML_MAX_MODULES 8u

typedef enum {
    DML_EVENT_TICK = 1,
    DML_EVENT_BUTTON,
    DML_EVENT_LIVE_VIEW_STARTED,
    DML_EVENT_LIVE_VIEW_STOPPED
} DmlEventType;

typedef struct {
    DmlEventType type;
    uint32_t millis;
    int button; /* simulator only; no known Nikon key codes */
} DmlEvent;

typedef struct {
    void *user;
    void (*log)(void *user, const char *message);
    int (*try_shutter)(void *user); /* 1 on success; mock implementation */
} DmlCameraAPI;

typedef struct {
    uint32_t abi_version;
    const char *name;
    void (*on_event)(void *state, const DmlEvent *event, const DmlCameraAPI *api);
    void (*describe)(const void *state, const DmlCameraAPI *api);
    void *state;
} DmlModule;

typedef struct {
    const DmlModule *modules[DML_MAX_MODULES];
    size_t count;
    const DmlCameraAPI *api;
} DmlRuntime;

void dml_init(DmlRuntime *runtime, const DmlCameraAPI *api);
int dml_register(DmlRuntime *runtime, const DmlModule *module);
void dml_dispatch(DmlRuntime *runtime, const DmlEvent *event);
void dml_describe(const DmlRuntime *runtime);

#endif

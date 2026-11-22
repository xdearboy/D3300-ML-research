#include "dml.h"

void dml_init(DmlRuntime *runtime, const DmlCameraAPI *api) {
    size_t i;
    runtime->api = api;
    runtime->count = 0;
    for (i = 0; i < DML_MAX_MODULES; ++i) runtime->modules[i] = 0;
}

int dml_register(DmlRuntime *runtime, const DmlModule *module) {
    size_t i;
    if (!runtime || !module || !module->name || !module->on_event ||
        module->abi_version != DML_ABI_VERSION || runtime->count >= DML_MAX_MODULES)
        return 0;
    for (i = 0; i < runtime->count; ++i) {
        if (runtime->modules[i] == module) return 0;
    }
    runtime->modules[runtime->count++] = module;
    return 1;
}

void dml_dispatch(DmlRuntime *runtime, const DmlEvent *event) {
    size_t i;
    if (!runtime || !event) return;
    for (i = 0; i < runtime->count; ++i) {
        const DmlModule *module = runtime->modules[i];
        module->on_event(module->state, event, runtime->api);
    }
}

void dml_describe(const DmlRuntime *runtime) {
    size_t i;
    if (!runtime) return;
    for (i = 0; i < runtime->count; ++i) {
        const DmlModule *module = runtime->modules[i];
        if (module->describe) module->describe(module->state, runtime->api);
    }
}

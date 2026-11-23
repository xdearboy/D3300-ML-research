#include "dml_tkernel.h"
#include <string.h>

#define DML_TK_SIM_MAX_MBF 8
#define DML_TK_SIM_MSG_MAX 16
#define DML_TK_SIM_MSG_LEN 32

typedef struct {
    uint8_t data[DML_TK_SIM_MSG_LEN];
    size_t  size;
} DmlTkSimMsg;

typedef struct {
    DmlTkSimMsg queue[DML_TK_SIM_MSG_MAX];
    size_t head;
    size_t tail;
    size_t count;
} DmlTkSimMbf;

static DmlTkSimMbf g_mbfs[DML_TK_SIM_MAX_MBF];

DmlTkError dml_tk_snd_mbf(DmlTkId mbfid, const void *msg, size_t msgsz, DmlTkTimeout tmout) {
    (void)tmout;
    if (!msg || msgsz == 0 || msgsz > DML_TK_SIM_MSG_LEN) return DML_TK_E_PAR;
    if (mbfid < 0 || mbfid >= DML_TK_SIM_MAX_MBF) return DML_TK_E_ID;

    DmlTkSimMbf *mbf = &g_mbfs[mbfid];
    if (mbf->count >= DML_TK_SIM_MSG_MAX) return DML_TK_E_QOVR;

    memcpy(mbf->queue[mbf->tail].data, msg, msgsz);
    mbf->queue[mbf->tail].size = msgsz;
    mbf->tail = (mbf->tail + 1) % DML_TK_SIM_MSG_MAX;
    mbf->count++;
    return DML_TK_E_OK;
}

DmlTkError dml_tk_rcv_mbf(DmlTkId mbfid, void *out_msg, DmlTkTimeout tmout) {
    (void)tmout;
    if (!out_msg) return DML_TK_E_PAR;
    if (mbfid < 0 || mbfid >= DML_TK_SIM_MAX_MBF) return DML_TK_E_ID;

    DmlTkSimMbf *mbf = &g_mbfs[mbfid];
    if (mbf->count == 0) return DML_TK_E_TMOUT;

    memcpy(out_msg, mbf->queue[mbf->head].data, mbf->queue[mbf->head].size);
    mbf->head = (mbf->head + 1) % DML_TK_SIM_MSG_MAX;
    mbf->count--;
    return DML_TK_E_OK;
}

DmlTkError dml_tk_dly_tsk(DmlTkTimeout millis) {
    (void)millis;
    return DML_TK_E_OK;
}

typedef enum {
    DML_TK_TSK_UNUSED = 0,
    DML_TK_TSK_DORMANT,
    DML_TK_TSK_READY,
    DML_TK_TSK_SUSPENDED
} DmlTkTaskState;

typedef struct {
    DmlTkTaskCreationPacket packet;
    DmlTkTaskState state;
    int32_t stacd;
} DmlTkSimTask;

static DmlTkSimTask g_tasks[DML_TK_MAX_TASKS];

typedef struct {
    int active;
    int locked;
    DmlTkAttr attr;
} DmlTkSimMutex;

static DmlTkSimMutex g_mutexes[DML_TK_MAX_MUTEXES];

DmlTkId dml_tk_cre_tsk(const DmlTkTaskCreationPacket *pk) {
    if (!pk) return DML_TK_E_PAR;

    for (uint32_t i = 0; i < DML_TK_MAX_TASKS; i++) {
        if (g_tasks[i].state == DML_TK_TSK_UNUSED) {
            g_tasks[i].packet = *pk;
            g_tasks[i].state = DML_TK_TSK_DORMANT;
            g_tasks[i].stacd = 0;
            return (DmlTkId)(i + 1u);
        }
    }
    return DML_TK_E_LIMIT;
}

DmlTkError dml_tk_sta_tsk(DmlTkId tskid, int32_t stacd) {
    if (tskid <= 0 || (uint32_t)tskid > DML_TK_MAX_TASKS) return DML_TK_E_ID;
    uint32_t idx = (uint32_t)tskid - 1u;
    if (g_tasks[idx].state != DML_TK_TSK_DORMANT) return DML_TK_E_OBJ;

    g_tasks[idx].state = DML_TK_TSK_READY;
    g_tasks[idx].stacd = stacd;
    if (g_tasks[idx].packet.task) {
        g_tasks[idx].packet.task(stacd, g_tasks[idx].packet.exinf);
    }
    return DML_TK_E_OK;
}

DmlTkError dml_tk_sus_tsk(DmlTkId tskid) {
    if (tskid <= 0 || (uint32_t)tskid > DML_TK_MAX_TASKS) return DML_TK_E_ID;
    uint32_t idx = (uint32_t)tskid - 1u;
    if (g_tasks[idx].state != DML_TK_TSK_READY) return DML_TK_E_OBJ;

    g_tasks[idx].state = DML_TK_TSK_SUSPENDED;
    return DML_TK_E_OK;
}

DmlTkError dml_tk_rsm_tsk(DmlTkId tskid) {
    if (tskid <= 0 || (uint32_t)tskid > DML_TK_MAX_TASKS) return DML_TK_E_ID;
    uint32_t idx = (uint32_t)tskid - 1u;
    if (g_tasks[idx].state != DML_TK_TSK_SUSPENDED) return DML_TK_E_OBJ;

    g_tasks[idx].state = DML_TK_TSK_READY;
    return DML_TK_E_OK;
}

void dml_tk_ext_tsk(void) {
    /* Exits currently executing task */
}

DmlTkId dml_tk_cre_mtx(const DmlTkMutexCreationPacket *pk) {
    for (uint32_t i = 0; i < DML_TK_MAX_MUTEXES; i++) {
        if (!g_mutexes[i].active) {
            g_mutexes[i].active = 1;
            g_mutexes[i].locked = 0;
            g_mutexes[i].attr = pk ? pk->mtxatr : 0;
            return (DmlTkId)(i + 1u);
        }
    }
    return DML_TK_E_LIMIT;
}

DmlTkError dml_tk_loc_mtx(DmlTkId mtxid, DmlTkTimeout tmout) {
    if (mtxid <= 0 || (uint32_t)mtxid > DML_TK_MAX_MUTEXES) return DML_TK_E_ID;
    uint32_t idx = (uint32_t)mtxid - 1u;
    if (!g_mutexes[idx].active) return DML_TK_E_NOEXS;

    if (g_mutexes[idx].locked) {
        if (tmout == DML_TK_TMO_POL) return DML_TK_E_TMOUT;
    }
    g_mutexes[idx].locked = 1;
    return DML_TK_E_OK;
}

DmlTkError dml_tk_unl_mtx(DmlTkId mtxid) {
    if (mtxid <= 0 || (uint32_t)mtxid > DML_TK_MAX_MUTEXES) return DML_TK_E_ID;
    uint32_t idx = (uint32_t)mtxid - 1u;
    if (!g_mutexes[idx].active) return DML_TK_E_NOEXS;

    g_mutexes[idx].locked = 0;
    return DML_TK_E_OK;
}

DmlTkError dml_tk_quiet_tasks(DmlTkQuietState *state, const DmlTkId *targets, size_t n_targets) {
    if (!state) return DML_TK_E_PAR;
    state->count = 0;
    if (!targets || n_targets == 0) return DML_TK_E_OK;

    for (size_t i = 0; i < n_targets && state->count < DML_TK_MAX_TASKS; i++) {
        DmlTkId tid = targets[i];
        if (dml_tk_sus_tsk(tid) == DML_TK_E_OK) {
            state->ids[state->count++] = tid;
        }
    }
    return DML_TK_E_OK;
}

DmlTkError dml_tk_unquiet_tasks(DmlTkQuietState *state) {
    if (!state) return DML_TK_E_PAR;

    for (uint32_t i = 0; i < state->count; i++) {
        dml_tk_rsm_tsk(state->ids[i]);
    }
    state->count = 0;
    return DML_TK_E_OK;
}


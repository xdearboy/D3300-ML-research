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

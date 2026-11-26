#ifndef DML_TKERNEL_H
#define DML_TKERNEL_H

#include <stddef.h>
#include <stdint.h>

/* Host-side T-Kernel model used by the simulator and unit tests.
   These declarations are not a recovered Nikon D3300 firmware ABI. */

typedef int32_t  DmlTkError;
typedef int32_t  DmlTkId;
typedef int32_t  DmlTkTimeout;
typedef uint32_t DmlTkAttr;
typedef int32_t  DmlTkPriority;

#define DML_TK_E_OK       0
#define DML_TK_E_PAR     (-17)
#define DML_TK_E_ID      (-18)
#define DML_TK_E_NOMEM   (-33)
#define DML_TK_E_LIMIT   (-34)
#define DML_TK_E_OBJ     (-41)
#define DML_TK_E_NOEXS   (-42)
#define DML_TK_E_QOVR    (-43)
#define DML_TK_E_TMOUT   (-50)

#define DML_TK_TMO_POL   0
#define DML_TK_TMO_FEVR (-1)

#define DML_TK_TA_HLNG   0x00000001u
#define DML_TK_TA_USERBUF 0x00000020u
#define DML_TK_TA_TFIFO  0x00000000u
#define DML_TK_TA_TPRI   0x00004000u

/* Task creation packet (T_CTSK) */
typedef struct {
    void          *exinf;      /* Extended task information */
    DmlTkAttr      tskatr;     /* Task attributes (e.g. TA_HLNG) */
    void         (*task)(int stacd, void *exinf); /* Task function entry */
    DmlTkPriority  itskpri;    /* Initial priority (1..TMaxPri) */
    size_t         stksz;      /* Stack size in bytes */
    void          *buf;        /* User stack buffer or NULL */
} DmlTkTaskCreationPacket;

/* Message buffer creation packet (T_CMBF) */
typedef struct {
    void          *exinf;      /* Extended information */
    DmlTkAttr      mbfatr;     /* Buffer attributes */
    size_t         bufsz;      /* Total buffer size in bytes */
    size_t         maxmsz;     /* Maximum message size */
    void          *buf;        /* Buffer memory pointer or NULL */
} DmlTkMsgBufferCreationPacket;

/* Mutex creation packet (T_CMTX) */
typedef struct {
    void          *exinf;      /* Extended information */
    DmlTkAttr      mtxatr;     /* Mutex attributes (e.g. TA_INHERIT) */
    DmlTkPriority  ceilpri;    /* Ceiling priority */
    uint32_t       reserved;
} DmlTkMutexCreationPacket;

#define DML_TK_TA_INHERIT 0x00000002u
#define DML_TK_MAX_TASKS  32u
#define DML_TK_MAX_MUTEXES 16u

/* Task state tracker for quieting/suspending background tasks */
typedef struct {
    uint32_t count;
    DmlTkId  ids[DML_TK_MAX_TASKS];
} DmlTkQuietState;

/* Task lifecycle API */
DmlTkId   dml_tk_cre_tsk(const DmlTkTaskCreationPacket *pk);
DmlTkError dml_tk_sta_tsk(DmlTkId tskid, int32_t stacd);
DmlTkError dml_tk_sus_tsk(DmlTkId tskid);
DmlTkError dml_tk_rsm_tsk(DmlTkId tskid);
void       dml_tk_ext_tsk(void);

/* Mutex API matching T-Kernel standard */
DmlTkId   dml_tk_cre_mtx(const DmlTkMutexCreationPacket *pk);
DmlTkError dml_tk_loc_mtx(DmlTkId mtxid, DmlTkTimeout tmout);
DmlTkError dml_tk_unl_mtx(DmlTkId mtxid);

/* Background task quieting API for clean display takeover */
DmlTkError dml_tk_quiet_tasks(DmlTkQuietState *state, const DmlTkId *targets, size_t n_targets);
DmlTkError dml_tk_unquiet_tasks(DmlTkQuietState *state);

/* Host-model message-buffer operations. */
DmlTkError dml_tk_snd_mbf(DmlTkId mbfid, const void *msg, size_t msgsz, DmlTkTimeout tmout);

DmlTkError dml_tk_rcv_mbf(DmlTkId mbfid, void *out_msg, DmlTkTimeout tmout);

/* Host-model task delay. */
DmlTkError dml_tk_dly_tsk(DmlTkTimeout millis);

#endif /* DML_TKERNEL_H */


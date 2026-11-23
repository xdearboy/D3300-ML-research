#ifndef DML_TKERNEL_H
#define DML_TKERNEL_H

#include <stddef.h>
#include <stdint.h>

/* Nikon D3300 C 1.02 T-Kernel 2.0 RTOS interface definitions.
   Verified against B-firmware strings at ROM 0x9154EE20 and kernel init at 0x9158B40C. */

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

/* Mailbox send message: models firmware 0x9002EC0E (snd_mbf) */
DmlTkError dml_tk_snd_mbf(DmlTkId mbfid, const void *msg, size_t msgsz, DmlTkTimeout tmout);

/* Mailbox receive message: models firmware rcv_mbf */
DmlTkError dml_tk_rcv_mbf(DmlTkId mbfid, void *out_msg, DmlTkTimeout tmout);

/* Task delay (sleep): models firmware dly_tsk */
DmlTkError dml_tk_dly_tsk(DmlTkTimeout millis);

#endif /* DML_TKERNEL_H */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../core/dml_tkernel.h"

static void test_mbf_lifecycle(void) {
    uint8_t tx_msg[16] = {0x02, 0x00, 0x00, 0x00, 0x12, 0x34};
    uint8_t rx_msg[16] = {0};

    /* Send to valid mailbox 0 */
    assert(dml_tk_snd_mbf(0, tx_msg, sizeof(tx_msg), 100) == DML_TK_E_OK);

    /* Receive from valid mailbox 0 */
    assert(dml_tk_rcv_mbf(0, rx_msg, 0) == DML_TK_E_OK);
    assert(memcmp(tx_msg, rx_msg, sizeof(tx_msg)) == 0);

    /* Receiving from empty mailbox returns timeout */
    assert(dml_tk_rcv_mbf(0, rx_msg, 0) == DML_TK_E_TMOUT);

    /* Invalid arguments */
    assert(dml_tk_snd_mbf(-1, tx_msg, sizeof(tx_msg), 0) == DML_TK_E_ID);
    assert(dml_tk_snd_mbf(0, NULL, sizeof(tx_msg), 0) == DML_TK_E_PAR);
    assert(dml_tk_snd_mbf(0, tx_msg, 0, 0) == DML_TK_E_PAR);
    assert(dml_tk_rcv_mbf(-1, rx_msg, 0) == DML_TK_E_ID);
    assert(dml_tk_rcv_mbf(0, NULL, 0) == DML_TK_E_PAR);

    /* Delay returns E_OK */
    assert(dml_tk_dly_tsk(10) == DML_TK_E_OK);
}

static int g_test_task_ran = 0;
static void dummy_task_entry(int stacd, void *exinf) {
    g_test_task_ran = 1;
    assert(stacd == 42);
    assert(exinf != NULL);
}

static void test_task_lifecycle_and_quieting(void) {
    int dummy_ctx = 123;
    DmlTkTaskCreationPacket pk = {
        .exinf = &dummy_ctx,
        .tskatr = DML_TK_TA_HLNG,
        .task = dummy_task_entry,
        .itskpri = 10,
        .stksz = 4096,
        .buf = NULL
    };

    /* Negative test */
    assert(dml_tk_cre_tsk(NULL) == DML_TK_E_PAR);

    /* Create task */
    DmlTkId tid1 = dml_tk_cre_tsk(&pk);
    assert(tid1 > 0);

    /* Start task */
    g_test_task_ran = 0;
    assert(dml_tk_sta_tsk(tid1, 42) == DML_TK_E_OK);
    assert(g_test_task_ran == 1);

    /* Cannot start already ready task */
    assert(dml_tk_sta_tsk(tid1, 42) == DML_TK_E_OBJ);

    /* Suspend and resume */
    assert(dml_tk_sus_tsk(tid1) == DML_TK_E_OK);
    /* Suspend again should fail because already suspended */
    assert(dml_tk_sus_tsk(tid1) == DML_TK_E_OBJ);
    assert(dml_tk_rsm_tsk(tid1) == DML_TK_E_OK);
    /* Resume again should fail because already ready */
    assert(dml_tk_rsm_tsk(tid1) == DML_TK_E_OBJ);

    /* Create second task to test quieting */
    DmlTkId tid2 = dml_tk_cre_tsk(&pk);
    assert(tid2 > 0);
    assert(dml_tk_sta_tsk(tid2, 42) == DML_TK_E_OK);

    /* Test quieting multiple background tasks */
    DmlTkQuietState qstate;
    DmlTkId targets[2] = {tid1, tid2};
    assert(dml_tk_quiet_tasks(&qstate, targets, 2) == DML_TK_E_OK);
    assert(qstate.count == 2);
    assert(qstate.ids[0] == tid1);
    assert(qstate.ids[1] == tid2);

    /* Both should now be suspended */
    assert(dml_tk_sus_tsk(tid1) == DML_TK_E_OBJ);
    assert(dml_tk_sus_tsk(tid2) == DML_TK_E_OBJ);

    /* Unquiet resumes all */
    assert(dml_tk_unquiet_tasks(&qstate) == DML_TK_E_OK);
    assert(qstate.count == 0);

    /* Both can be suspended again */
    assert(dml_tk_sus_tsk(tid1) == DML_TK_E_OK);
    assert(dml_tk_sus_tsk(tid2) == DML_TK_E_OK);
}

static void test_mutex_lifecycle(void) {
    DmlTkMutexCreationPacket pk = {
        .exinf = NULL,
        .mtxatr = DML_TK_TA_INHERIT,
        .ceilpri = 0,
        .reserved = 0
    };

    DmlTkId mtx = dml_tk_cre_mtx(&pk);
    assert(mtx > 0);

    /* Lock mutex */
    assert(dml_tk_loc_mtx(mtx, DML_TK_TMO_FEVR) == DML_TK_E_OK);

    /* Polling lock on locked mutex returns timeout */
    assert(dml_tk_loc_mtx(mtx, DML_TK_TMO_POL) == DML_TK_E_TMOUT);

    /* Unlock mutex */
    assert(dml_tk_unl_mtx(mtx) == DML_TK_E_OK);

    /* Lock again after unlock */
    assert(dml_tk_loc_mtx(mtx, DML_TK_TMO_POL) == DML_TK_E_OK);
    assert(dml_tk_unl_mtx(mtx) == DML_TK_E_OK);

    /* Invalid mutex ID */
    assert(dml_tk_loc_mtx(-1, 0) == DML_TK_E_ID);
    assert(dml_tk_unl_mtx(-1) == DML_TK_E_ID);
    assert(dml_tk_loc_mtx(99, 0) == DML_TK_E_ID);
}

int main(void) {
    test_mbf_lifecycle();
    test_task_lifecycle_and_quieting();
    test_mutex_lifecycle();
    puts("All T-Kernel RTOS tests passed successfully.");
    return 0;
}


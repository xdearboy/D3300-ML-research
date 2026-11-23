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

int main(void) {
    test_mbf_lifecycle();
    puts("All T-Kernel RTOS tests passed successfully.");
    return 0;
}

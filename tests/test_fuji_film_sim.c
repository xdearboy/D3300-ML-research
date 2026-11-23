#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../modules/fuji_film_sim.h"

static void test_fuji_profiles_definition(void) {
    for (int id = 0; id < (int)DML_FUJI_COUNT; id++) {
        const DmlFujiProfile *p = dml_fuji_get_profile((DmlFujiProfileId)id);
        assert(p != NULL);
        assert(p->name != NULL && strlen(p->name) > 0);
        assert(p->desc != NULL && strlen(p->desc) > 0);
        assert(p->base_profile_id != 0);

        /* Validate LUT curve properties */
        assert(p->lut[0] <= 10000); /* Black point lift within reason */
        assert(p->lut[255] == 65535); /* White point full scale */
        for (int i = 0; i < 255; i++) {
            /* Monotonically non-decreasing */
            assert(p->lut[i] <= p->lut[i + 1]);
        }
    }

    /* Out of bounds returns NULL */
    assert(dml_fuji_get_profile(DML_FUJI_COUNT) == NULL);
    assert(dml_fuji_get_profile((DmlFujiProfileId)99) == NULL);
}

static void test_fuji_ncp_generation(void) {
    uint8_t buf[DML_NCP_FILE_SIZE];
    size_t out_sz = 0;
    const DmlFujiProfile *classic_chrome = dml_fuji_get_profile(DML_FUJI_CLASSIC_CHROME);
    assert(classic_chrome != NULL);

    /* Null buffer or insufficient buffer checks */
    assert(dml_fuji_generate_ncp(NULL, buf, sizeof(buf), &out_sz) != 0);
    assert(dml_fuji_generate_ncp(classic_chrome, NULL, sizeof(buf), &out_sz) != 0);
    assert(dml_fuji_generate_ncp(classic_chrome, buf, DML_NCP_FILE_SIZE - 1, &out_sz) != 0);

    /* Valid generation */
    assert(dml_fuji_generate_ncp(classic_chrome, buf, sizeof(buf), &out_sz) == 0);
    assert(out_sz == DML_NCP_FILE_SIZE);

    /* Check Header: NCP\0 */
    assert(buf[0] == 'N' && buf[1] == 'C' && buf[2] == 'P' && buf[3] == 0x00);

    /* Record 0x01: Tag 1 (Big Endian uint32), Size 36 */
    assert(buf[4] == 0x00 && buf[5] == 0x00 && buf[6] == 0x00 && buf[7] == 0x01);
    assert(buf[8] == 0x00 && buf[9] == 0x00 && buf[10] == 0x00 && buf[11] == 36);

    /* Name inside Record 0x01 at offset 16 */
    assert(strncmp((const char *)&buf[16], "CLASSIC CHROME", 14) == 0);

    /* Base Profile ID inside Record 0x01 at offset 36 (Big Endian uint16) */
    uint16_t base_id = (uint16_t)((buf[36] << 8) | buf[37]);
    assert(base_id == classic_chrome->base_profile_id);

    /* Record 0x02: Tag 2 (Big Endian uint32), Size 640 */
    /* Header (4) + Rec1 Tag(4) + Rec1 Len(4) + Rec1 Data(36) = 48 */
    assert(buf[48] == 0x00 && buf[49] == 0x00 && buf[50] == 0x00 && buf[51] == 0x02);
    assert(buf[52] == 0x00 && buf[53] == 0x00 && buf[54] == 0x02 && buf[55] == 0x80); /* 640 = 0x0280 */

    /* Record 0x00: Tag 0x00000000 at offset 696 (48 + 8 + 640 = 696) */
    assert(buf[696] == 0x00 && buf[697] == 0x00 && buf[698] == 0x00 && buf[699] == 0x00);
}

static void test_fuji_export_to_sd(void) {
    const char *test_sd = "/tmp/dml_test_fuji_sd";
    const DmlFujiProfile *velvia = dml_fuji_get_profile(DML_FUJI_VELVIA);
    assert(velvia != NULL);

    int res = dml_fuji_export_to_sd(velvia, test_sd, 2);
    assert(res == 0);

    /* Check PICCON02.NOP */
    FILE *f_nop = fopen("/tmp/dml_test_fuji_sd/NIKON/CUSTOMPC/PICCON02.NOP", "rb");
    assert(f_nop != NULL);
    fseek(f_nop, 0, SEEK_END);
    long sz_nop = ftell(f_nop);
    fclose(f_nop);
    assert(sz_nop == (long)DML_NCP_FILE_SIZE);

    /* Check PICCON02.NCP */
    FILE *f_ncp = fopen("/tmp/dml_test_fuji_sd/NIKON/CUSTOMPC/PICCON02.NCP", "rb");
    assert(f_ncp != NULL);
    fseek(f_ncp, 0, SEEK_END);
    long sz_ncp = ftell(f_ncp);
    fclose(f_ncp);
    assert(sz_ncp == (long)DML_NCP_FILE_SIZE);
}

static void test_fuji_export_all(void) {
    const char *test_dir = "/tmp/dml_test_fuji_all";
    int res = dml_fuji_export_all(test_dir);
    assert(res == 0);

    for (int i = 1; i <= 5; i++) {
        char nop_path[256];
        char ncp_path[256];
        snprintf(nop_path, sizeof(nop_path), "%s/NIKON/CUSTOMPC/PICCON%02d.NOP", test_dir, i);
        snprintf(ncp_path, sizeof(ncp_path), "%s/NIKON/CUSTOMPC/PICCON%02d.NCP", test_dir, i);

        FILE *f1 = fopen(nop_path, "rb");
        assert(f1 != NULL);
        fseek(f1, 0, SEEK_END);
        assert(ftell(f1) == (long)DML_NCP_FILE_SIZE);
        fclose(f1);

        FILE *f2 = fopen(ncp_path, "rb");
        assert(f2 != NULL);
        fseek(f2, 0, SEEK_END);
        assert(ftell(f2) == (long)DML_NCP_FILE_SIZE);
        fclose(f2);
    }
}

int main(void) {
    test_fuji_profiles_definition();
    test_fuji_ncp_generation();
    test_fuji_export_to_sd();
    test_fuji_export_all();
    printf("All Fuji Film Sim tests passed successfully!\n");
    return 0;
}

#ifndef DML_CAMERA_STATUS_HW_H
#define DML_CAMERA_STATUS_HW_H

#include <stddef.h>
#include <stdint.h>
#include "dml_camera_status.h"

/* Verified hardware memory addresses for Nikon D3300 C 1.02 B-firmware */
#define D3300_ADDR_CKRRC_CAMERA_STATUS 0x10491580u
#define D3300_ADDR_CKRRC_VTABLE        0x9151F6F0u
#define D3300_ADDR_CINFO_SHOOTING_MENU 0x915073A4u
#define D3300_ADDR_POWER_MANAGER       0x912838A0u

/* Raw memory layout of CKRRCCameraStatus in Nikon D3300 RAM (0x10491580)
   Verified against constructor at 0x9019A340 and state updater at 0x9019A6B0..0x9019A740 */
typedef struct {
    uint32_t vtable;                   /* 0x00: points to 0x9151F6F0 */
    uint8_t  pad_0x04[0x7C];           /* 0x04..0x7F: internal task / mutex states */
    uint32_t mode_flags;               /* 0x80: bits 0..3 shooting mode */
    uint32_t sub_object_ptr;           /* 0x84: pointer to exposure sub-object */
    uint8_t  pad_0x88[0x04];           /* 0x88..0x8B */
    uint8_t  tv_code;                  /* 0x8C: APEX / index shutter speed */
    uint8_t  av_code;                  /* 0x8D: APEX / index aperture */
    uint8_t  iso_code;                 /* 0x8E: ISO index (0..9) + auto bit (bit 7) */
    int8_t   ev_compensation;          /* 0x8F: EV compensation in 1/3 EV (-9..+9) */
    uint8_t  metering_mode;            /* 0x90: 0=Matrix, 1=Center-weighted, 2=Spot */
    uint8_t  af_mode;                  /* 0x91: 0=AF-S, 1=AF-C, 2=AF-A, 3=MF */
    uint8_t  active_focus_point;       /* 0x92: 0=Center, 1..10=outer array */
    uint8_t  release_mode;             /* 0x93: 0=Single, 1=Continuous, 2=Quiet, 3=Timer */
    uint8_t  flash_state;              /* 0x94: bit 7=raised, bits 0..3=flash mode */
    int8_t   flash_compensation;       /* 0x95: flash EV compensation in 1/3 EV */
    uint8_t  battery_status;           /* 0x96: bits 0..6=percent 0..100, bit 7=warning flag */
    uint8_t  image_quality;            /* 0x97: 0=RAW+F, 1=RAW, 2=F, 3=N, 4=B */
    uint32_t shots_remaining;          /* 0x98: remaining frames */
    uint8_t  buffer_remaining;         /* 0x9C: remaining buffer shots */
    uint8_t  white_balance;            /* 0x9D: white balance code */
    uint8_t  pad_0x9E[2];
} DmlHwCameraStatusRaw;

/* Reads and unpacks live camera parameters from hardware RAM into DmlCameraStatus.
   If raw_ptr is NULL, on ARM targets it defaults to D3300_ADDR_CKRRC_CAMERA_STATUS.
   Returns 0 on success, -1 on invalid parameter or uninitialized memory. */
int dml_camera_status_read_hw(DmlCameraStatus *out_status, const void *raw_ptr);

/* Conversion helper functions */
void dml_hw_map_shutter(uint8_t tv_code, uint32_t *num, uint32_t *den, uint8_t *is_bulb);
uint16_t dml_hw_map_aperture(uint8_t av_code);
uint32_t dml_hw_map_iso(uint8_t iso_code, uint8_t *is_auto);

#endif /* DML_CAMERA_STATUS_HW_H */

#ifndef DML_DRAW_BUFFER_H
#define DML_DRAW_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/* Verified Nikon D3300 C 1.02 DrawBuffer API and pixel pipeline.
   Reverse-engineered from 0x9029E720..0x9029EB50 in B-firmware.
   Descriptor table located at RAM 0x104A0868 (20 entries of 20 bytes each). */

#define DML_DRAW_BUFFER_MAX 20u
#define DML_DRAW_BUFFER_ENTRY_SIZE 20u

/* Query parameter codes from table at ROM 0x9147FAF4 + 0x28 */
#define DML_DRAW_BUFFER_QUERY_ADDR   0x3100u /* Framebuffer memory address */
#define DML_DRAW_BUFFER_QUERY_WIDTH  0x3101u /* Buffer width in pixels */
#define DML_DRAW_BUFFER_QUERY_HEIGHT 0x3103u /* Buffer height in pixels */
#define DML_DRAW_BUFFER_QUERY_SYNC   0x3110u /* Vsync synchronization status */
#define DML_DRAW_BUFFER_QUERY_STRIDE 0x3111u /* Stride / pitch in bytes */
#define DML_DRAW_BUFFER_QUERY_FORMAT 0x3112u /* Pixel format (32 bpp ARGB8888) */

/* Stride alignment formula confirmed at 0x9029EA80..0x9029EA92 and 0x901297F2:
   ((width + 7) / 8) * 32 bytes = ((width + 7) & ~7) * 4 bytes per scanline */
#define DML_DRAW_BUFFER_STRIDE(w) ((((uint32_t)(w) + 7u) & ~7u) * 4u)

/* 20-byte descriptor matching hardware table at 0x104A0868 */
typedef struct {
    uint32_t active;      /* offset 0x00: 1 if allocated, 0 if free */
    uint32_t handle;      /* offset 0x04: buffer handle */
    uint32_t mem_id;      /* offset 0x08: memory allocation handle */
    uint16_t width;       /* offset 0x0C: width in pixels */
    uint16_t height;      /* offset 0x0E: height in pixels */
    uint32_t stride;      /* offset 0x10: bytes per scanline */
    uint32_t *pixels;     /* host/device pixel pointer */
} DmlDrawBufferEntry;

/* Complete table container matching 20 entries at 0x104A0868 */
typedef struct {
    DmlDrawBufferEntry entries[DML_DRAW_BUFFER_MAX];
    int current_selected; /* Selected buffer index (0..19), or -1 if none */
} DmlDrawBufferTable;

/* Initializes table to all-zero (matching startup state) */
void dml_draw_buffer_init(DmlDrawBufferTable *table);

/* Releases any allocated host pixel buffers */
void dml_draw_buffer_cleanup(DmlDrawBufferTable *table);

/* Verified 0x9029E720: DrawBufferCreate.
   Finds first free slot, computes stride, allocates buffer, returns index (0..19) or -1 on error. */
int dml_draw_buffer_create(DmlDrawBufferTable *table, uint16_t width, uint16_t height);

/* Verified 0x9029E848: DrawBufferSelect.
   Verifies slot is active, sets current_selected, returns 0 on success, -1 on error. */
int dml_draw_buffer_select(DmlDrawBufferTable *table, int index);

/* Verified 0x9029E8B0: DrawBufferDestroy.
   Frees resources, clears active flag. If selected, unselects buffer. Returns 0 on success. */
int dml_draw_buffer_destroy(DmlDrawBufferTable *table, int index);

/* Verified 0x9029E940: DrawBufferSwap.
   Performs display buffer swap, handles sync flag. Returns 0 on success, -1 on error. */
int dml_draw_buffer_swap(DmlDrawBufferTable *table, int index, int sync_vsync);

/* Verified 0x9029E9B2: DrawBufferQuery.
   Queries field for active buffer using query code. Returns 0 on success, -1 on error. */
int dml_draw_buffer_query(const DmlDrawBufferTable *table, int index, uint16_t query_code, void *out_value);

/* UI & OSD rasterizer primitives on ARGB8888 buffer */
void dml_draw_buffer_clear(DmlDrawBufferTable *table, int index, uint32_t argb_color);
void dml_draw_buffer_fill_rect(DmlDrawBufferTable *table, int index, int x, int y, int w, int h, uint32_t argb_color);
void dml_draw_buffer_draw_rect(DmlDrawBufferTable *table, int index, int x, int y, int w, int h, uint32_t border_argb);
void dml_draw_buffer_draw_line_h(DmlDrawBufferTable *table, int index, int x, int y, int length, uint32_t argb_color);
void dml_draw_buffer_draw_line_v(DmlDrawBufferTable *table, int index, int x, int y, int length, uint32_t argb_color);
void dml_draw_buffer_draw_char(DmlDrawBufferTable *table, int index, int x, int y, char c, uint32_t fg_argb, uint32_t bg_argb);
void dml_draw_buffer_draw_char_scaled(DmlDrawBufferTable *table, int index, int x, int y, char c, uint32_t fg_argb, uint32_t bg_argb, uint8_t scale);
void dml_draw_buffer_draw_text(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb);
void dml_draw_buffer_draw_text_scaled(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb, uint8_t scale);

/* Export framebuffer to standard 24-bit uncompressed BMP image.
   Returns 0 on success, -1 on invalid parameter or file I/O error. */
int dml_draw_buffer_save_bmp(const DmlDrawBufferTable *table, int index, const char *filepath);

/* Export framebuffer to standard binary Netpbm P6 PPM image.
   Returns 0 on success, -1 on invalid parameter or file I/O error. */
int dml_draw_buffer_save_ppm(const DmlDrawBufferTable *table, int index, const char *filepath);

#endif /* DML_DRAW_BUFFER_H */


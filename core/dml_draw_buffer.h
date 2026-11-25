#ifndef DML_DRAW_BUFFER_H
#define DML_DRAW_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/* Host-side DrawBuffer model informed by routines at 0x9029E720..0x9029EB50.
   Static analysis confirms a table at RAM 0x104A0868 with 20-byte entries, but
   exact field semantics, pixel format and live framebuffer addresses are open. */

#define DML_DRAW_BUFFER_MAX 20u
#define DML_DRAW_BUFFER_ENTRY_SIZE 20u

/* Query parameter codes from table at ROM 0x9147FAF4 + 0x28 */
#define DML_DRAW_BUFFER_QUERY_ADDR   0x3100u /* Observed selector; host model uses address */
#define DML_DRAW_BUFFER_QUERY_WIDTH  0x3101u /* Observed selector; host model uses width */
#define DML_DRAW_BUFFER_QUERY_HEIGHT 0x3103u /* Observed selector; host model uses height */
#define DML_DRAW_BUFFER_QUERY_SYNC   0x3110u /* Observed selector; hardware meaning unproven */
#define DML_DRAW_BUFFER_QUERY_STRIDE 0x3111u /* Observed selector; host model uses stride */
#define DML_DRAW_BUFFER_QUERY_FORMAT 0x3112u /* Observed selector; host model returns ARGB8888 */

/* This is a host-model stride, not a recovered firmware pixel ABI. */
#define DML_DRAW_BUFFER_STRIDE(w) ((((uint32_t)(w) + 7u) & ~7u) * 4u)

/* Simulator entry. This deliberately does not claim ABI compatibility with the
   camera's observed 20-byte table: the host-only pixel pointer makes it larger. */
typedef struct {
    uint32_t active;      /* offset 0x00: 1 if allocated, 0 if free */
    uint32_t handle;      /* offset 0x04: buffer handle */
    uint32_t mem_id;      /* offset 0x08: memory allocation handle */
    uint16_t width;       /* offset 0x0C: width in pixels */
    uint16_t height;      /* offset 0x0E: height in pixels */
    uint32_t stride;      /* offset 0x10: bytes per scanline */
    uint32_t *pixels;     /* host-only pixel pointer */
} DmlDrawBufferEntry;

/* Host-side table container. */
typedef struct {
    DmlDrawBufferEntry entries[DML_DRAW_BUFFER_MAX];
    int current_selected; /* Selected buffer index (0..19), or -1 if none */
} DmlDrawBufferTable;

/* Initializes table to all-zero (matching startup state) */
void dml_draw_buffer_init(DmlDrawBufferTable *table);

/* Releases any allocated host pixel buffers */
void dml_draw_buffer_cleanup(DmlDrawBufferTable *table);

/* Host model of the observed DrawBufferCreate behavior. */
int dml_draw_buffer_create(DmlDrawBufferTable *table, uint16_t width, uint16_t height);

/* Host model of the observed DrawBufferSelect behavior. */
int dml_draw_buffer_select(DmlDrawBufferTable *table, int index);

/* Host model of the observed DrawBufferDestroy behavior. */
int dml_draw_buffer_destroy(DmlDrawBufferTable *table, int index);

/* Host model of the observed DrawBufferSwap behavior. */
int dml_draw_buffer_swap(DmlDrawBufferTable *table, int index, int sync_vsync);

/* Host model of the observed DrawBufferQuery behavior. */
int dml_draw_buffer_query(const DmlDrawBufferTable *table, int index, uint16_t query_code, void *out_value);

/* Typography & Font Engine */
typedef enum {
    DML_FONT_MODERN_SANS = 0, /* Clean proportional sans-serif (14px, smooth) */
    DML_FONT_MONO_TECH   = 1, /* Classic 8x8 fixed-pitch industrial monospace */
    DML_FONT_RETRO_MATRIX = 2, /* Authentic OSD dot-matrix */
    DML_FONT_COUNT       = 3
} DmlFontId;

/* Select active font family for dml_draw_buffer_draw_text */
void dml_draw_buffer_set_font(DmlFontId font_id);
DmlFontId dml_draw_buffer_get_font(void);

/* Calculate exact width in pixels of text rendered with font_id */
int dml_draw_buffer_get_text_width(const char *text, DmlFontId font_id);

/* Host-only UI rasterizer primitives on its ARGB8888 buffer. */
void dml_draw_buffer_clear(DmlDrawBufferTable *table, int index, uint32_t argb_color);
void dml_draw_buffer_put_pixel(DmlDrawBufferTable *table, int index, int x, int y, uint32_t argb_color);
void dml_draw_buffer_fill_rect(DmlDrawBufferTable *table, int index, int x, int y, int w, int h, uint32_t argb_color);
void dml_draw_buffer_draw_rect(DmlDrawBufferTable *table, int index, int x, int y, int w, int h, uint32_t border_argb);
void dml_draw_buffer_draw_line_h(DmlDrawBufferTable *table, int index, int x, int y, int length, uint32_t argb_color);
void dml_draw_buffer_draw_line_v(DmlDrawBufferTable *table, int index, int x, int y, int length, uint32_t argb_color);
void dml_draw_buffer_draw_char(DmlDrawBufferTable *table, int index, int x, int y, char c, uint32_t fg_argb, uint32_t bg_argb);
void dml_draw_buffer_draw_char_scaled(DmlDrawBufferTable *table, int index, int x, int y, char c, uint32_t fg_argb, uint32_t bg_argb, uint8_t scale);
void dml_draw_buffer_draw_text(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb);
void dml_draw_buffer_draw_text_scaled(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb, uint8_t scale);
void dml_draw_buffer_draw_text_font(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb, DmlFontId font_id);
void dml_draw_buffer_draw_text_sub(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb);

/* Export framebuffer to standard 24-bit uncompressed BMP image.
   Returns 0 on success, -1 on invalid parameter or file I/O error. */
int dml_draw_buffer_save_bmp(const DmlDrawBufferTable *table, int index, const char *filepath);

/* Export framebuffer to standard binary Netpbm P6 PPM image.
   Returns 0 on success, -1 on invalid parameter or file I/O error. */
int dml_draw_buffer_save_ppm(const DmlDrawBufferTable *table, int index, const char *filepath);

#endif /* DML_DRAW_BUFFER_H */


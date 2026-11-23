#ifndef DML_FONTS_DATA_H
#define DML_FONTS_DATA_H

#include <stdint.h>

#define DML_GLYPH_MAX_ROWS 15

typedef struct {
    uint8_t  advance;                    /* Total horizontal advance in pixels */
    uint16_t rows[DML_GLYPH_MAX_ROWS];   /* Scanline bitmasks (MSB is left pixel at pen position) */
} DmlGlyph;

extern const DmlGlyph g_font_sans14[95];
extern const DmlGlyph g_font_sans10[95];
extern const uint8_t g_font_retro_matrix[95][7];

#endif /* DML_FONTS_DATA_H */

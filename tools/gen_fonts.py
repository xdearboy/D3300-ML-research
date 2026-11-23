#!/usr/bin/env python3
"""
Generates C font tables for D3300-ML from TrueType/OpenType sources.
Produces:
- core/dml_fonts_data.h
- core/dml_fonts_data.c
"""

import os
from PIL import Image, ImageFont, ImageDraw

def gen_font(pt, font_path, height, name, thresh=80):
    font = ImageFont.truetype(font_path, pt)
    lines = []
    lines.append(f"/* Auto-generated proportional font: {name} ({pt}pt, height {height}px) */")
    lines.append(f"const DmlGlyph g_{name}[95] = {{")
    for code in range(32, 127):
        ch = chr(code)
        adv = int(round(font.getlength(ch)))
        if ch == ' ':
            adv = max(adv, 4 if pt <= 11 else 5)
            rows = [0] * height
        else:
            img = Image.new('L', (adv + 4, height), 0)
            d = ImageDraw.Draw(img)
            d.text((0, 0), ch, font=font, fill=255)
            rows = []
            for y in range(height):
                mask = 0
                for x in range(min(adv + 2, 16)):
                    if img.getpixel((x, y)) > thresh:
                        mask |= (1 << (15 - x))
                rows.append(mask)

        rows_str = ", ".join(f"0x{m:04X}u" for m in rows)
        safe_ch = ch
        if safe_ch == '\\':
            safe_ch = "\\\\"
        elif safe_ch == '\'':
            safe_ch = "\\'"
        lines.append(f"    {{{adv}, {{{rows_str}}}}}, /* [{code}] '{safe_ch}' */")
    lines.append("};")
    return "\n".join(lines)

def main():
    font_path = "/System/Library/Fonts/Helvetica.ttc"
    if not os.path.exists(font_path):
        font_path = "/System/Library/Fonts/Supplemental/Arial.ttf"

    header_content = """#ifndef DML_FONTS_DATA_H
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
"""
    with open("core/dml_fonts_data.h", "w") as f:
        f.write(header_content)

    c_content = [
        '#include "dml_fonts_data.h"',
        '',
        gen_font(14, font_path, 15, "font_sans14", thresh=80),
        '',
        gen_font(10, font_path, 12, "font_sans10", thresh=85),
        '',
        '/* Authentic 5x7 dot matrix font for RETRO_MATRIX style */',
        'const uint8_t g_font_retro_matrix[95][7] = {',
    ]

    retro_font = ImageFont.load_default()
    for code in range(32, 127):
        ch = chr(code)
        img = Image.new('L', (8, 8), 0)
        d = ImageDraw.Draw(img)
        d.text((0, 0), ch, font=retro_font, fill=255)
        r_bytes = []
        for y in range(7):
            b = 0
            for x in range(5):
                if img.getpixel((x, y)) > 100:
                    b |= (1 << (4 - x))
            r_bytes.append(f"0x{b:02X}u")
        safe_ch = ch
        if safe_ch == '\\':
            safe_ch = "\\\\"
        elif safe_ch == '\'':
            safe_ch = "\\'"
        c_content.append(f"    {{{', '.join(r_bytes)}}}, /* [{code}] '{safe_ch}' */")
    c_content.append('};')
    c_content.append('')

    with open("core/dml_fonts_data.c", "w") as f:
        f.write("\n".join(c_content))

    print("Regenerated core/dml_fonts_data.h and core/dml_fonts_data.c with natural baseline typography.")

if __name__ == "__main__":
    main()

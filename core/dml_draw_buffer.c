#include "dml_draw_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard basic 8x8 font bitmap table for printable ASCII 32..126 */
static const uint8_t g_font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* '!' */
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* '"' */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* '#' */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* '$' */
    {0x00,0x66,0xA6,0xD4,0x2B,0x65,0x66,0x00}, /* '%' */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* '\'' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* '.' */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* '/' */
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, /* '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* '1' */
    {0x7C,0xC6,0x06,0x1C,0x30,0x60,0xFE,0x00}, /* '2' */
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, /* '3' */
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, /* '4' */
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, /* '5' */
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, /* '6' */
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, /* '7' */
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, /* '8' */
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, /* '9' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* ':' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* ';' */
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, /* '<' */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* '=' */
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, /* '>' */
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, /* '?' */
    {0x7C,0xC6,0xDE,0xDE,0xDC,0xC0,0x7C,0x00}, /* '@' */
    {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* 'A' */
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, /* 'B' */
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, /* 'C' */
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, /* 'D' */
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, /* 'E' */
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, /* 'F' */
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3A,0x00}, /* 'G' */
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* 'H' */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 'I' */
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, /* 'J' */
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, /* 'K' */
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, /* 'L' */
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, /* 'M' */
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, /* 'N' */
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 'O' */
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, /* 'P' */
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xCE,0x7C,0x06}, /* 'Q' */
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, /* 'R' */
    {0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00}, /* 'S' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 'T' */
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 'U' */
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, /* 'V' */
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, /* 'W' */
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, /* 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, /* 'Y' */
    {0xFE,0xC6,0x0C,0x18,0x30,0x66,0xFE,0x00}, /* 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* '[' */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ']' */
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, /* '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* '_' */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* '`' */
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, /* 'a' */
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, /* 'b' */
    {0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00}, /* 'c' */
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, /* 'd' */
    {0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00}, /* 'e' */
    {0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00}, /* 'f' */
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, /* 'g' */
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, /* 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* 'i' */
    {0x0C,0x00,0x1C,0x0C,0x0C,0xCC,0xCC,0x78}, /* 'j' */
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, /* 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 'l' */
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, /* 'm' */
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, /* 'n' */
    {0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00}, /* 'o' */
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, /* 'p' */
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, /* 'q' */
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, /* 'r' */
    {0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00}, /* 's' */
    {0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, /* 't' */
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, /* 'u' */
    {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00}, /* 'v' */
    {0x00,0x00,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, /* 'w' */
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, /* 'x' */
    {0x00,0x00,0xC6,0xC6,0xCE,0x78,0x0C,0xF8}, /* 'y' */
    {0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00}, /* 'z' */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* '|' */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* '}' */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  /* '~' */
};

void dml_draw_buffer_init(DmlDrawBufferTable *table) {
    if (!table) return;
    memset(table, 0, sizeof(*table));
    table->current_selected = -1;
}

void dml_draw_buffer_cleanup(DmlDrawBufferTable *table) {
    if (!table) return;
    for (uint32_t i = 0; i < DML_DRAW_BUFFER_MAX; i++) {
        if (table->entries[i].active && table->entries[i].pixels) {
            free(table->entries[i].pixels);
            table->entries[i].pixels = NULL;
        }
    }
    memset(table, 0, sizeof(*table));
    table->current_selected = -1;
}

int dml_draw_buffer_create(DmlDrawBufferTable *table, uint16_t width, uint16_t height) {
    if (!table || width == 0 || height == 0) return -1;

    for (uint32_t i = 0; i < DML_DRAW_BUFFER_MAX; i++) {
        if (!table->entries[i].active) {
            uint32_t stride = DML_DRAW_BUFFER_STRIDE(width);
            size_t total_bytes = (size_t)stride * (size_t)height;
            uint32_t *pixels = (uint32_t *)calloc(1, total_bytes);
            if (!pixels) return -1;

            table->entries[i].active = 1;
            table->entries[i].handle = 0x2000u + i;
            table->entries[i].mem_id = 0x4000u + i;
            table->entries[i].width = width;
            table->entries[i].height = height;
            table->entries[i].stride = stride;
            table->entries[i].pixels = pixels;
            return (int)i;
        }
    }
    return -1;
}

int dml_draw_buffer_select(DmlDrawBufferTable *table, int index) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return -1;
    if (!table->entries[index].active) return -1;

    table->current_selected = index;
    return 0;
}

int dml_draw_buffer_destroy(DmlDrawBufferTable *table, int index) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return -1;
    if (!table->entries[index].active) return -1;

    if (table->current_selected == index) {
        table->current_selected = -1;
    }

    if (table->entries[index].pixels) {
        free(table->entries[index].pixels);
        table->entries[index].pixels = NULL;
    }

    memset(&table->entries[index], 0, sizeof(table->entries[index]));
    return 0;
}

int dml_draw_buffer_swap(DmlDrawBufferTable *table, int index, int sync_vsync) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return -1;
    if (!table->entries[index].active) return -1;
    (void)sync_vsync;
    return 0;
}

int dml_draw_buffer_query(const DmlDrawBufferTable *table, int index, uint16_t query_code, void *out_value) {
    if (!table || !out_value || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return -1;
    if (!table->entries[index].active) return -1;

    const DmlDrawBufferEntry *entry = &table->entries[index];
    switch (query_code) {
        case DML_DRAW_BUFFER_QUERY_ADDR:
            *(uintptr_t *)out_value = (uintptr_t)entry->pixels;
            return 0;
        case DML_DRAW_BUFFER_QUERY_WIDTH:
            *(uint16_t *)out_value = entry->width;
            return 0;
        case DML_DRAW_BUFFER_QUERY_HEIGHT:
            *(uint16_t *)out_value = entry->height;
            return 0;
        case DML_DRAW_BUFFER_QUERY_SYNC:
            *(uint32_t *)out_value = 1;
            return 0;
        case DML_DRAW_BUFFER_QUERY_STRIDE:
            *(uint32_t *)out_value = entry->stride;
            return 0;
        case DML_DRAW_BUFFER_QUERY_FORMAT:
            *(uint32_t *)out_value = 0x04; /* 32bpp ARGB8888 */
            return 0;
        default:
            return -1;
    }
}

void dml_draw_buffer_clear(DmlDrawBufferTable *table, int index, uint32_t argb_color) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return;
    if (!table->entries[index].active || !table->entries[index].pixels) return;

    DmlDrawBufferEntry *entry = &table->entries[index];
    uint32_t words_per_stride = entry->stride / 4u;
    for (uint16_t y = 0; y < entry->height; y++) {
        uint32_t *row = entry->pixels + y * words_per_stride;
        for (uint16_t x = 0; x < entry->width; x++) {
            row[x] = argb_color;
        }
    }
}

void dml_draw_buffer_fill_rect(DmlDrawBufferTable *table, int index, int x, int y, int w, int h, uint32_t argb_color) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX || w <= 0 || h <= 0) return;
    if (!table->entries[index].active || !table->entries[index].pixels) return;

    DmlDrawBufferEntry *entry = &table->entries[index];
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > (int)entry->width ? (int)entry->width : (x + w);
    int y1 = (y + h) > (int)entry->height ? (int)entry->height : (y + h);

    if (x0 >= x1 || y0 >= y1) return;

    uint32_t words_per_stride = entry->stride / 4u;
    for (int cy = y0; cy < y1; cy++) {
        uint32_t *row = entry->pixels + cy * words_per_stride;
        for (int cx = x0; cx < x1; cx++) {
            row[cx] = argb_color;
        }
    }
}

void dml_draw_buffer_draw_char(DmlDrawBufferTable *table, int index, int x, int y, char c, uint32_t fg_argb, uint32_t bg_argb) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX) return;
    if (!table->entries[index].active || !table->entries[index].pixels) return;

    if (c < 32 || (unsigned char)c > 126) c = '?';
    const uint8_t *glyph = g_font8x8[(unsigned char)c - 32];

    DmlDrawBufferEntry *entry = &table->entries[index];
    uint32_t words_per_stride = entry->stride / 4u;

    for (int row = 0; row < 8; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)entry->height) continue;

        uint8_t bits = glyph[row];
        uint32_t *row_pixels = entry->pixels + py * words_per_stride;

        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)entry->width) continue;

            if (bits & (0x80u >> col)) {
                row_pixels[px] = fg_argb;
            } else if ((bg_argb & 0xFF000000u) != 0) {
                row_pixels[px] = bg_argb;
            }
        }
    }
}

void dml_draw_buffer_draw_text(DmlDrawBufferTable *table, int index, int x, int y, const char *text, uint32_t fg_argb, uint32_t bg_argb) {
    if (!table || !text) return;
    int cur_x = x;
    while (*text) {
        if (*text == '\n') {
            cur_x = x;
            y += 8;
        } else {
            dml_draw_buffer_draw_char(table, index, cur_x, y, *text, fg_argb, bg_argb);
            cur_x += 8;
        }
        text++;
    }
}

int dml_draw_buffer_save_bmp(const DmlDrawBufferTable *table, int index, const char *filepath) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX || !filepath) {
        return -1;
    }
    const DmlDrawBufferEntry *entry = &table->entries[index];
    if (!entry->active || !entry->pixels || entry->width == 0 || entry->height == 0) {
        return -1;
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        return -1;
    }

    uint32_t width = entry->width;
    uint32_t height = entry->height;
    uint32_t row_stride = ((width * 3u + 3u) / 4u) * 4u;
    uint32_t image_size = row_stride * height;
    uint32_t file_size = 54u + image_size;

    /* BMP File Header (14 bytes) */
    uint8_t file_header[14];
    file_header[0] = 0x42; /* 'B' */
    file_header[1] = 0x4D; /* 'M' */
    file_header[2] = (uint8_t)(file_size & 0xFFu);
    file_header[3] = (uint8_t)((file_size >> 8) & 0xFFu);
    file_header[4] = (uint8_t)((file_size >> 16) & 0xFFu);
    file_header[5] = (uint8_t)((file_size >> 24) & 0xFFu);
    file_header[6] = 0;
    file_header[7] = 0;
    file_header[8] = 0;
    file_header[9] = 0;
    file_header[10] = 54; /* 54-byte offset to pixel array */
    file_header[11] = 0;
    file_header[12] = 0;
    file_header[13] = 0;

    /* DIB Header (BITMAPINFOHEADER - 40 bytes) */
    uint8_t dib_header[40];
    memset(dib_header, 0, sizeof(dib_header));
    dib_header[0] = 40; /* Header size */
    dib_header[4] = (uint8_t)(width & 0xFFu);
    dib_header[5] = (uint8_t)((width >> 8) & 0xFFu);
    dib_header[6] = (uint8_t)((width >> 16) & 0xFFu);
    dib_header[7] = (uint8_t)((width >> 24) & 0xFFu);
    dib_header[8] = (uint8_t)(height & 0xFFu);
    dib_header[9] = (uint8_t)((height >> 8) & 0xFFu);
    dib_header[10] = (uint8_t)((height >> 16) & 0xFFu);
    dib_header[11] = (uint8_t)((height >> 24) & 0xFFu);
    dib_header[12] = 1;  /* Planes = 1 */
    dib_header[14] = 24; /* 24 bpp */
    dib_header[20] = (uint8_t)(image_size & 0xFFu);
    dib_header[21] = (uint8_t)((image_size >> 8) & 0xFFu);
    dib_header[22] = (uint8_t)((image_size >> 16) & 0xFFu);
    dib_header[23] = (uint8_t)((image_size >> 24) & 0xFFu);
    dib_header[24] = 0x13; /* 2835 ppm (~72 dpi) */
    dib_header[25] = 0x0B;
    dib_header[28] = 0x13;
    dib_header[29] = 0x0B;

    if (fwrite(file_header, 1, sizeof(file_header), fp) != sizeof(file_header) ||
        fwrite(dib_header, 1, sizeof(dib_header), fp) != sizeof(dib_header)) {
        fclose(fp);
        return -1;
    }

    /* Allocate one padded row buffer */
    uint8_t *row_buf = (uint8_t *)calloc(1, row_stride);
    if (!row_buf) {
        fclose(fp);
        return -1;
    }

    uint32_t words_per_stride = entry->stride / 4u;
    int write_error = 0;

    /* BMP raster data is stored bottom-up */
    for (int y = (int)height - 1; y >= 0; y--) {
        const uint32_t *src_row = entry->pixels + y * (int)words_per_stride;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pixel = src_row[x];
            row_buf[x * 3u + 0u] = (uint8_t)(pixel & 0xFFu);         /* Blue */
            row_buf[x * 3u + 1u] = (uint8_t)((pixel >> 8) & 0xFFu);  /* Green */
            row_buf[x * 3u + 2u] = (uint8_t)((pixel >> 16) & 0xFFu); /* Red */
        }
        if (fwrite(row_buf, 1, row_stride, fp) != row_stride) {
            write_error = 1;
            break;
        }
    }

    free(row_buf);
    fclose(fp);
    return write_error ? -1 : 0;
}

int dml_draw_buffer_save_ppm(const DmlDrawBufferTable *table, int index, const char *filepath) {
    if (!table || index < 0 || (uint32_t)index >= DML_DRAW_BUFFER_MAX || !filepath) {
        return -1;
    }
    const DmlDrawBufferEntry *entry = &table->entries[index];
    if (!entry->active || !entry->pixels || entry->width == 0 || entry->height == 0) {
        return -1;
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        return -1;
    }

    /* Netpbm P6 binary RGB format */
    if (fprintf(fp, "P6\n%u %u\n255\n", (unsigned int)entry->width, (unsigned int)entry->height) < 0) {
        fclose(fp);
        return -1;
    }

    uint32_t width = entry->width;
    uint32_t height = entry->height;
    uint32_t row_bytes = width * 3u;
    uint8_t *row_buf = (uint8_t *)malloc(row_bytes);
    if (!row_buf) {
        fclose(fp);
        return -1;
    }

    uint32_t words_per_stride = entry->stride / 4u;
    int write_error = 0;

    for (uint32_t y = 0; y < height; y++) {
        const uint32_t *src_row = entry->pixels + y * words_per_stride;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pixel = src_row[x];
            row_buf[x * 3u + 0u] = (uint8_t)((pixel >> 16) & 0xFFu); /* Red */
            row_buf[x * 3u + 1u] = (uint8_t)((pixel >> 8) & 0xFFu);  /* Green */
            row_buf[x * 3u + 2u] = (uint8_t)(pixel & 0xFFu);         /* Blue */
        }
        if (fwrite(row_buf, 1, row_bytes, fp) != row_bytes) {
            write_error = 1;
            break;
        }
    }

    free(row_buf);
    fclose(fp);
    return write_error ? -1 : 0;
}


#include "pixels_operation.h"
#include "fontdata.h"
#include <string.h>

void copy_row2(u16 *src, int src_w, u16 *dst, int dst_w) {
    u16 pixel = 0;
    int pos = 0x10000;
    int inc = (src_w << 16) / dst_w;
    for (int i = dst_w; i > 0; --i) {
        while (pos >= 0x10000L) {
            pixel = *src++;
            pos -= 0x10000L;
        }
        
        *dst++ = pixel;
        pos += inc;
    }
};

void stretch_rgb565_buffer(void *src, void *dst, int sw, int sh, int dw, int dh) {
    int pos = 0x10000, inc = (sh << 16) / dh;
    int src_row = 0, dst_row = 0;
    int spitch = sw * 2, dpitch = dw * 2;
    u8 *srcp = NULL, *dstp = NULL;
    
    for (int dst_maxrow = dst_row + dh; dst_row < dst_maxrow; ++dst_row) {
        dstp = (u8 *) dst + (dst_row * dpitch);
        while (pos >= 0x10000L) {
            srcp = (u8 *) src + (src_row * spitch);
            ++src_row;
            pos -= 0x10000L;
        }
        
        copy_row2((u16 *) srcp, sw, (u16 *) dstp, dw);
        pos += inc;
    }
}

void rgb565_to_rgb8888(void *src, void* dst, int w, int h) {
    u16 *s = src;
    u32 *d = dst;
    
    for (int i = 0; i < w * h; i++, s++, d++) {
        u16 color = *s;
        u8 r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
        u8 g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
        u8 b = (((color & 0x1F) * 527) + 23) >> 6;
        *d = RGBA8(r, g, b, 0xFF);
    }
}

void draw_char(void *buf, int buf_w, int x, int y, unsigned char a, int r, int g, int b) {
    for (int h = 8; h; h--) {
        u32 *dst = (u32 *)buf + (y + 8 - h) * buf_w + x;
        for (int w = 8; w; w--) {
            u32 color = *dst;
            
            if ((fontdata8x8[a * 8 + (8 - h)] >> w) & 1) color = RGBA8(r, g, b, 0xFF);
            *dst++ = color;
        }
    }
}

void draw_string(void *buf, int buf_w, char *str, int x, int y, int r, int g, int b) {
    size_t len = strlen(str);
    for (int i = 0; i < len; i++, x += 6) {
        draw_char(buf, buf_w, x, y, str[i], r, g, b);
    }
}

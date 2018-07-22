#include <switch.h>

extern void stretch_rgb565_buffer(void *src, void *dst, int sw, int sh, int dw, int dh);
extern void rgb565_to_rgb8888(void *src, void* dst, int w, int h);
extern void draw_string(void *buf, int buf_w, char *str, int x, int y, int r, int g, int b);

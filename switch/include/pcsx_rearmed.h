#include "plugin_lib.h"
#include <switch.h>

extern short switch_input(void);
extern void switch_flip(void *buf_ptr, int w, int h, int fps);

extern void pcsx_rearmed_init(char *dir, int frameskip);
extern bool pcsx_rearmed_load_game(char *path);
extern void pcsx_rearmed_run(void);
extern void pcsx_rearmed_stop(void);
extern void pcsx_rearmed_close(void);

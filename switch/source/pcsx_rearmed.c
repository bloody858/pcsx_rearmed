#include "pcsx_rearmed.h"
#include "pcsx_mem.h"
#include "cspace.h"
#include "psxcounters.h"
#include "plugin.h"
#include "plugins.h"
#include "main.h"
#include "psemu_plugin_defs.h"
#include "new_dynarec/new_dynarec.h"
#include "dfsound/spu_config.h"
#include "dfinput/externals.h"
#include <stdlib.h>
#include <sys/time.h>

/* PCSX ReARMed core calls and stuff */
int in_analog_left[8][2] = {{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 }};
int in_analog_right[8][2] = {{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 },{ 127, 127 }};

int multitap1 = 0;
int multitap2 = 0;
int in_enable_vibration = 1;

#define PORTS_NUMBER 8
unsigned short in_keystate[PORTS_NUMBER];

int in_type[8] = {
    PSE_PAD_TYPE_NONE, PSE_PAD_TYPE_NONE,
    PSE_PAD_TYPE_NONE, PSE_PAD_TYPE_NONE,
    PSE_PAD_TYPE_NONE, PSE_PAD_TYPE_NONE,
    PSE_PAD_TYPE_NONE, PSE_PAD_TYPE_NONE };

/* PSX max resolution is 640x512, but with enhancement it's 1024x512 */
#define VOUT_MAX_WIDTH 1024
#define VOUT_MAX_HEIGHT 512

static int vout_width, vout_height;
static int vout_doffs_old, vout_fb_dirty;
static int plugins_opened = 0;
static int is_pal = 0, frame_interval = 0, frame_interval1024 = 0;
static int vsync_cnt = 0, vsync_usec_time = 0;
static void *vout_buf;
static void *vout_buf_ptr;

#define tvdiff(tv, tv_old) \
    ((tv.tv_sec - tv_old.tv_sec) * 1000000 + tv.tv_usec - tv_old.tv_usec)

void pl_frame_limit(void) {
    /* called once per frame, make psxCpu->Execute() above return */
    //stop = 1;
    
    static struct timeval tv_old, tv_expect;
    static int vsync_cnt_prev, drc_active_vsyncs;
    int diff;
    
    struct timeval now;
    gettimeofday(&now, 0);
    
    vsync_cnt++;
    
    if (now.tv_sec != tv_old.tv_sec) {
        diff = tvdiff(now, tv_old);
        //printf("%i\n", diff);
        pl_rearmed_cbs.vsps_cur = 0.0f;
        if (0 < diff && diff <= 2000000)
            pl_rearmed_cbs.vsps_cur = 1000000.0f * (vsync_cnt - vsync_cnt_prev) / diff;
        
        vsync_cnt_prev = vsync_cnt;
        pl_rearmed_cbs.flips_per_sec = pl_rearmed_cbs.flip_cnt;
        pl_rearmed_cbs.flip_cnt = 0;
        tv_old = now;
    }
    
    // tv_expect uses usec*1024 units instead of usecs for better accuracy
    tv_expect.tv_usec += frame_interval1024;
    if (tv_expect.tv_usec >= (1000000 << 10)) {
        tv_expect.tv_usec -= (1000000 << 10);
        tv_expect.tv_sec++;
    }
    
    if (pl_rearmed_cbs.frameskip) {
        if (diff < -frame_interval)
            pl_rearmed_cbs.fskip_advice = 1;
        else if (diff >= 0)
            pl_rearmed_cbs.fskip_advice = 0;
        
        // recompilation is not that fast and may cause frame skip on
        // loading screens and such, resulting in flicker or glitches
        if (new_dynarec_did_compile) {
            if (drc_active_vsyncs < 32)
                pl_rearmed_cbs.fskip_advice = 0;
            drc_active_vsyncs++;
        }
        else
            drc_active_vsyncs = 0;
        new_dynarec_did_compile = 0;
    }
    
    in_keystate[0] = switch_input();
}

void pl_timing_prepare(int is_pal_) {
    pl_rearmed_cbs.fskip_advice = 0;
    pl_rearmed_cbs.flips_per_sec = 0;
    pl_rearmed_cbs.cpu_usage = 0;
    
    is_pal = is_pal_;
    frame_interval = is_pal ? 20000 : 16667;
    frame_interval1024 = is_pal ? 20000*1024 : 17066667;
}

void plat_trigger_vibrate(int pad, int low, int high) {
    if(in_enable_vibration) {
        
    }
}

void pl_update_gun(int *xn, int *yn, int *xres, int *yres, int *in) {}

static int vout_open(void) { return 0; }
static void vout_close(void) {}

static void vout_set_mode(int w, int h, int raw_w, int raw_h, int bpp) {
    vout_width = w;
    vout_height = h;
}

static void vout_flip(const void *vram, int stride, int bgr24, int w, int h) {
    unsigned short *dest = vout_buf_ptr;
    const unsigned short *src = vram;
    int dstride = vout_width, h1 = h;
    
    if (vram == NULL) {
        // blanking
        memset(vout_buf_ptr, 0, dstride * h * 2);
        return;
    }
    
    int doffs = (vout_height - h) * dstride;
    doffs += (dstride - w) / 2 & ~1;
    if (doffs != vout_doffs_old) {
        // clear borders
        memset(vout_buf_ptr, 0, dstride * h * 2);
        vout_doffs_old = doffs;
    }
    dest += doffs;
    
    if (bgr24) {
        // XXX: could we switch to RETRO_PIXEL_FORMAT_XRGB8888 here?
        for (; h1-- > 0; dest += dstride, src += stride) {
            bgr888_to_rgb565(dest, src, w * 3);
        }
    }
    else {
        for (; h1-- > 0; dest += dstride, src += stride) {
            bgr555_to_rgb565(dest, src, w * 2);
        }
    }
    
    switch_flip(vout_buf_ptr, w, h, pl_rearmed_cbs.flips_per_sec);
    pl_rearmed_cbs.flip_cnt++;
}

struct rearmed_cbs pl_rearmed_cbs = {
    .pl_vout_open = vout_open,
    .pl_vout_set_mode = vout_set_mode,
    .pl_vout_flip = vout_flip,
    .pl_vout_close = vout_close,
    .mmap = pl_mmap,
    .munmap = pl_munmap,
    /* from psxcounters */
    .gpu_hcnt = &hSyncCount,
    .gpu_frame_count = &frame_counter,
};

static bool try_use_bios(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    if (size != 512 * 1024)
        return false;
    
    char *name = strrchr(path, '/');
    if (name++ == NULL)
        name = path;
    snprintf(Config.Bios, sizeof(Config.Bios), "%s", name);
    return true;
}

void pcsx_rearmed_init(char *dir, int frameskip) {
    const char *bios[] = {
        "SCPH101", "SCPH7001", "SCPH5501", "SCPH1001",
        "scph101", "scph7001", "scph5501", "scph1001"
    };
    int ret = 0;
    
    vout_buf = malloc(VOUT_MAX_WIDTH * VOUT_MAX_HEIGHT * 2);
    vout_buf_ptr = vout_buf;
    
    ret = emu_core_preinit();
    
    Config.Cpu = CPU_INTERPRETER;
    
    snprintf(Config.Mcd1, sizeof(Config.Mcd1), "%s/card1.mcd", dir);
    snprintf(Config.Mcd2, sizeof(Config.Mcd2), "%s/card2.mcd", dir);
    
    ret |= emu_core_init();
    if (ret != 0) {
        SysPrintf("PCSX init failed.\n");
        exit(1);
    }
    
    bool found_bios = false;
    snprintf(Config.BiosDir, sizeof(Config.BiosDir), "%s", dir);
    
    for (int i = 0; i < sizeof(bios) / sizeof(bios[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s.bin", dir, bios[i]);
        found_bios = try_use_bios(path);
        
        if (found_bios)
            break;
    }
    
    if (found_bios) {
        SysPrintf("found BIOS file: %s\n", Config.Bios);
    }
    else {
        SysPrintf("no BIOS files found.\n");
    }
    
    /* Set how much slower PSX CPU runs * 100 (so that 200 is 2 times)
     * we have to do this because cache misses and some IO penalties
     * are not emulated. Warning: changing this may break compatibility. */
    cycle_multiplier = 175;
    
    pl_rearmed_cbs.gpu_peops.iUseDither = 1;
    spu_config.iUseFixedUpdates = 1;
    
    pl_rearmed_cbs.frameskip = frameskip;
    Config.PsxAuto = 0;
}

bool pcsx_rearmed_load_game(char *path) {
    if (plugins_opened) {
        ClosePlugins();
        plugins_opened = 0;
    }
    
    set_cd_image(path);
    
    /* have to reload after set_cd_image for correct cdr plugin */
    if (LoadPlugins() == -1) {
        SysPrintf("failed to load plugins\n");
        return false;
    }
    
    plugins_opened = 1;
    NetOpened = 0;
    
    if (OpenPlugins() == -1) {
        SysPrintf("failed to open plugins\n");
        return false;
    }
    
    plugin_call_rearmed_cbs();
    dfinput_activate();
    
    if (CheckCdrom() == -1) {
        SysPrintf("unsupported/invalid CD image: %s\n", path);
        return false;
    }
    
    SysReset();
    
    if (LoadCdrom() == -1) {
        SysPrintf("could not load CD\n");
        return false;
    }
    emu_on_new_cd(0);
    return true;
}

void pcsx_rearmed_run() {
    stop = 0;
    psxCpu->Execute();
}

void pcsx_rearmed_stop() {
    stop = 1;
}

void pcsx_rearmed_close() {
    SysClose();
    free(vout_buf);
    vout_buf = NULL;
}

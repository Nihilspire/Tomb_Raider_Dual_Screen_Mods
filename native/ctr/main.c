/* main.c - Tomb Raider Dual 3DS (alpha): the pcsx_rearmed core on the top screen, Tomb Raider Link (linkscr.c) on the bottom touch
 * screen. From Xenogears Dual 3DS's frontend (Xenogears\xenogears-dual\native\ctr\main.c: same core, same build of it), with
 * Tomb Raider's disc, a digital pad and the Link screen's touches. libctru only: both screens are RGB565 framebuffers.
 *
 * The player's disc goes in the same folder as the .3dsx (a .cia looks in sdmc:/3ds/tomb-raider-dual, then sdmc:/3ds): .cue
 * (with its .bin), .chd or .pbp. tomb-raider-link.pak beside it gives the map (tools/mk_pak.js makes it from the same disc). A real
 * BIOS (scph*.bin) beside them is used if there is one. The memory card, the states, the Link settings and tomb-raider-3ds.log
 * (the numbers to read back after a run on the real machine) are written there too.
 *
 * Buttons: A circle, B cross, X triangle, Y square, L/R L1/R1, ZL/ZR L2/R2, Circle Pad as the D-pad. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <dirent.h>
#include "libretro.h"
#include "linkscr.h"
#ifdef CTR_TEST
#include "test_script.h"     /* written by test/build_test.sh: #define TEST_SCRIPT {...}, {...} and maybe TEST_STATE, TEST_QUIT, TEST_SHOT */
#endif

#define APP_VERSION "0.1-alpha"

/* ---------- what the core expects of the program it's linked into ---------- */
u32 __stacksize__ = 2 * 1024 * 1024;   /* the recompiler keeps large tables on the stack */
int __ctr_svchax;                      /* 1 = the recompiler may run: main() sets it (see "the recompiler's cache") */
extern char CdromId[10];
extern unsigned char McdFlag[2];

/* ------------------------------------------------------------------ the recompiler's cache
 * The recompiler must flush the CPU's caches after writing code. The core does that through Luma3DS's svcCustomBackdoor (svc 0x80),
 * which a real console with Luma has and Azahar doesn't. Azahar has Luma's newer cache calls, and those tell its own recompiler to
 * throw away what it compiled from the changed memory. The Makefile renames the core's versions to hw_*. */
extern void hw_clear_cache(void);
extern void hw_clear_cache_range(void *start, void *end);
extern void hw_invalidate_icache(void);
static int onEmulator;
static inline void emuInvalidateAll(void) { __asm__ volatile("svc 0x94" ::: "r0", "r1", "r2", "r3", "r12", "lr", "memory"); }
static inline void emuInvalidate(void *p, u32 n) {
    register void *a __asm__("r0") = p; register u32 b __asm__("r1") = n;
    __asm__ volatile("svc 0x93" : "+r"(a), "+r"(b) :: "r2", "r3", "r12", "lr", "memory");
}
void ctr_clear_cache(void) { if (onEmulator) emuInvalidateAll(); else hw_clear_cache(); }
void ctr_clear_cache_range(void *s, void *e) { if (onEmulator) emuInvalidate(s, (u32)((char *)e - (char *)s)); else hw_clear_cache_range(s, e); }
void ctr_invalidate_icache(void) { if (onEmulator) emuInvalidateAll(); else hw_invalidate_icache(); }
void wait_for_input(void) {}

/* ------------------------------------------------------------------ the log */
static char DIRP[256];
static FILE *logf; static int logLines;
static void logs(const char *s) {
    if (!logf || logLines > 4000) return;
    fputs(s, logf); fputc('\n', logf); fflush(logf); logLines++;
}
static void logp(const char *fmt, ...) {
    char t[300]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap); logs(t);
}
static void log_cb(enum retro_log_level level, const char *fmt, ...) {
    char t[240]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    size_t n = strlen(t); while (n && (t[n - 1] == '\n' || t[n - 1] == '\r')) t[--n] = 0;
    if (n) logs(t);
}

/* ------------------------------------------------------------------ libretro callbacks */
/* the settings Tomb Raider Dual gives the core on a phone, with Unai's choices for this machine (as Xenogears Dual 3DS) */
static const char *const VARS[][2] = {
    { "pcsx_rearmed_bios", "auto" }, { "pcsx_rearmed_drc", "enabled" },
    { "pcsx_rearmed_memcard1", "libretro" }, { "pcsx_rearmed_memcard2", "disabled" },
    { "pcsx_rearmed_display_fps_v2", "disabled" }, { "pcsx_rearmed_show_bios_bootlogo", "disabled" },
    { "pcsx_rearmed_spu_interpolation", "simple" }, { "pcsx_rearmed_dithering", "disabled" },
    { "pcsx_rearmed_spu_thread", "enabled" }, { "pcsx_rearmed_gpu_thread_rendering", "auto" }, { "pcsx_rearmed_cd_readahead", "12" },
    { "pcsx_rearmed_3ds_appcputime", "35" },
    { "pcsx_rearmed_gpu_unai_old_renderer", "disabled" }, { "pcsx_rearmed_gpu_unai_skipline", "disabled" },
    { "pcsx_rearmed_gpu_unai_lighting", "enabled" }, { "pcsx_rearmed_gpu_unai_fast_lighting", "disabled" },
    { "pcsx_rearmed_gpu_unai_blending", "enabled" },
};
static int frameskip, varsDirty;
static char skipVal[8] = "0";
static struct retro_system_av_info av;
static char sysDir[256];

static bool env_cb(unsigned cmd, void *data) {
    switch (cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data = sysDir; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(const enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        if (!strcmp(v->key, "pcsx_rearmed_frameskip_type")) { v->value = frameskip ? "fixed_interval" : "disabled"; return true; }
        if (!strcmp(v->key, "pcsx_rearmed_frameskip_interval")) { snprintf(skipVal, sizeof skipVal, "%d", frameskip ? frameskip : 1); v->value = skipVal; return true; }
        for (size_t i = 0; i < sizeof VARS / sizeof VARS[0]; i++) if (!strcmp(v->key, VARS[i][0])) { v->value = VARS[i][1]; return true; }
        v->value = NULL; return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)data = varsDirty; varsDirty = 0; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((struct retro_log_callback *)data)->log = log_cb; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
    case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: *(unsigned *)data = 1; return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: memcpy(&av, data, sizeof av); return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY: memcpy(&av.geometry, data, sizeof av.geometry); return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: *(unsigned *)data = 0; return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE: { const struct retro_message *m = data; if (m && m->msg) logs(m->msg); return true; }
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)data = 0; return true;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    case RETRO_ENVIRONMENT_SET_VARIABLES: case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        return true;
    }
    return false;
}

/* ---- the picture: 3DS framebuffers are stored rotated, pixel (x, y) of the 400x240 screen at fb[x * 240 + 239 - y] ---- */
static int freshFrame, lastW = -1, lastH = -1, clearTop;
static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) {
    static int colOf[400], rowOf[240], dw, dh, ox, oy;
    if (!data || !w || !h) return;                                 /* a repeat: keep the last picture */
    if ((int)w != lastW || (int)h != lastH) {                      /* worked out once: the ARM11 has no divide instruction */
        lastW = w; lastH = h; clearTop = 2;
        dw = w > 400 ? 400 : w; dh = h > 240 ? 240 : h;            /* wider or taller modes: nearest pixel down to the screen */
        ox = (400 - dw) / 2; oy = (240 - dh) / 2;
        for (int x = 0; x < dw; x++) colOf[x] = x * w / dw;
        for (int y = 0; y < dh; y++) rowOf[y] = y * h / dh;
    }
    uint16_t *fb = (uint16_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    if (clearTop) { memset(fb, 0, 400 * 240 * 2); clearTop--; }
    const uint16_t *src = data; int pp = (int)(pitch / 2);
    for (int x = 0; x < dw; x++) {
        const uint16_t *col = src + colOf[x];
        uint16_t *dst = fb + (ox + x) * 240 + 239 - oy;
        if (dh == (int)h) { for (int y = 0; y < dh; y++) { dst[-y] = *col; col += pp; } }
        else for (int y = 0; y < dh; y++) dst[-y] = col[rowOf[y] * pp];
    }
    freshFrame = 1;
}

/* ---- sound through NDSP; silent when the DSP firmware dump is missing ---- */
#define NBUF 8
#define BUFSAMP 1024                          /* stereo frames per buffer, ~23 ms at 44100 Hz */
static ndspWaveBuf wb[NBUF];
static int16_t *sbuf; static int sfill, scur, haveSound;
static void soundInit(double rate) {
    if (R_FAILED(ndspInit())) { logs("no NDSP (dump the DSP firmware for sound)"); return; }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)rate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    sbuf = (int16_t *)linearAlloc(NBUF * BUFSAMP * 4);
    if (!sbuf) { ndspExit(); return; }
    memset(wb, 0, sizeof wb);
    for (int i = 0; i < NBUF; i++) { wb[i].data_vaddr = sbuf + i * BUFSAMP * 2; wb[i].nsamples = BUFSAMP; wb[i].status = NDSP_WBUF_DONE; }
    haveSound = 1;
}
static size_t audio_batch_cb(const int16_t *s, size_t frames) {
    if (!haveSound) return frames;
    for (size_t i = 0; i < frames; i++) {
        if (wb[scur].status != NDSP_WBUF_DONE && wb[scur].status != NDSP_WBUF_FREE) return frames;   /* the DSP is behind: drop */
        int16_t *d = (int16_t *)wb[scur].data_vaddr;
        d[sfill * 2] = s[i * 2]; d[sfill * 2 + 1] = s[i * 2 + 1];
        if (++sfill == BUFSAMP) {
            DSP_FlushDataCache(d, BUFSAMP * 4);
            ndspChnWaveBufAdd(0, &wb[scur]);
            scur = (scur + 1) % NBUF; sfill = 0;
        }
    }
    return frames;
}
static void audio_cb(int16_t l, int16_t r) { int16_t s[2] = { l, r }; audio_batch_cb(s, 1); }

/* ---- input: the standard digital pad, as the game expects ---- */
static uint32_t padBits;
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD) return 0;
    return id == RETRO_DEVICE_ID_JOYPAD_MASK ? (int16_t)padBits : (padBits >> id) & 1;
}

/* ------------------------------------------------------------------ files */
#define MCD_BYTES 131072
static char SRM[320], STATE0[320], STATE1[320], DISC[320];
static void formatCard(uint8_t *d) {                               /* as format_card in psxlink.c */
    unsigned off = 0, i;
    memset(d, 0, MCD_BYTES);
    d[off++] = 'M'; d[off++] = 'C'; off += 0x7d; d[off++] = 0x0e;
    for (i = 0; i < 15; i++) { d[off++] = 0xa0; off += 0x07; d[off++] = 0xff; d[off++] = 0xff; off += 0x75; d[off++] = 0xa0; }
    for (i = 0; i < 20; i++) { d[off++] = 0xff; d[off++] = 0xff; d[off++] = 0xff; d[off++] = 0xff; off += 0x04; d[off++] = 0xff; d[off++] = 0xff; off += 0x76; }
}
static int saveFile(const char *path, const void *b, size_t n) {   /* .tmp then rename: a crash never leaves half a file */
    char tmp[340]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb"); if (!f) return 0;
    int ok = fwrite(b, 1, n, f) == n; fclose(f);
    if (!ok) { remove(tmp); return 0; }
    remove(path);
    return rename(tmp, path) == 0;
}
static int saveState(const char *p) {
    size_t n = retro_serialize_size(); void *b = n ? malloc(n) : NULL; int ok = 0;
    if (b && retro_serialize(b, n)) ok = saveFile(p, b, n);
    free(b); return ok;
}
static int loadState(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    void *b = malloc(n); int ok = b && fread(b, 1, n, f) == n && retro_unserialize(b, n);
    free(b); fclose(f);
    return ok;
}
static const char *const DISCEXT[] = { ".cue", ".chd", ".pbp", NULL };
static int isDisc(const char *n) {
    size_t l = strlen(n); if (l < 5 || n[0] == '.') return 0;
    for (int i = 0; DISCEXT[i]; i++) if (!strcasecmp(n + l - 4, DISCEXT[i])) return i + 1;
    return 0;
}
static int scanDir(const char *dir) {
    DIR *d = opendir(dir); struct dirent *e; int rank = 99;
    if (!d) return 0;
    while ((e = readdir(d))) {
        int k = isDisc(e->d_name); if (!k || k >= rank) continue;
        rank = k; snprintf(DISC, sizeof DISC, "%s/%s", dir, e->d_name);
    }
    closedir(d);
    return rank != 99;
}
static int findDisc(int argc, char **argv) {
    char dir[256] = "";
    if (argc > 0 && argv && argv[0] && strchr(argv[0], '/')) {     /* the Homebrew Launcher: "sdmc:/3ds/tomb-raider-dual/Tomb Raider Dual 3DS.3dsx" */
        snprintf(dir, sizeof dir, "%s", argv[0]); *strrchr(dir, '/') = 0;
        if (!strchr(dir, '/')) strcat(dir, "/");
        if (scanDir(dir)) { snprintf(DIRP, sizeof DIRP, "%s", dir); return 1; }
    }
    static const char *const TRY[] = { "sdmc:/3ds/tomb-raider-dual", "sdmc:/3ds" };   /* a .cia has no folder of its own */
    for (int i = 0; i < 2; i++) if (scanDir(TRY[i])) { snprintf(DIRP, sizeof DIRP, "%s", TRY[i]); return 1; }
    snprintf(DIRP, sizeof DIRP, "%s", dir[0] ? dir : TRY[0]);
    return 0;
}
static void waitExit(const char *msg) {
    consoleInit(GFX_BOTTOM, NULL);
    printf("\n  TOMB RAIDER DUAL 3DS (" APP_VERSION ")\n\n%s\n\n  Press START to quit.\n", msg);
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
}

/* ------------------------------------------------------------------ */
#define SCREEN_HZ 59.83                                            /* the 3DS screen's refresh */
#define BTN(x) (1u << (x))

int main(int argc, char **argv) {
    gfxInitDefault();
    osSetSpeedupEnable(true);                                      /* New 3DS: 804 MHz and the L2 cache */
    bool n3ds = false; APT_CheckNew3DS(&n3ds);

    /* The recompiler needs Luma3DS's calls on a real machine. Without them the core falls back to its (very slow) interpreter. */
    s64 v = 0, ve = 0;
    Result re = svcGetSystemInfo(&ve, 0x20000, 0);
    onEmulator = R_SUCCEEDED(re) && ve != 0;
    Result rl = svcGetSystemInfo(&v, 0x10000, 0);
    int luma = R_SUCCEEDED(rl) && v != 0;
    __ctr_svchax = onEmulator || luma;
    cfguInit();                                                    /* the core counts the CPU's cores from the system model */

    int haveDisc = findDisc(argc, argv);
    { char p[320]; snprintf(p, sizeof p, "%s/tomb-raider-3ds.log", DIRP); logf = fopen(p, "w"); }
    logp("Tomb Raider Dual 3DS %s: %s, %s, luma %d (%08lx %lld) emulator %d (%08lx %lld), folder %s", APP_VERSION, n3ds ? "new 3ds" : "old 3ds", __ctr_svchax ? "recompiler" : "interpreter", luma, rl, v, onEmulator, re, ve, DIRP);
    if (!haveDisc) {
        char msg[400];
        snprintf(msg, sizeof msg, "  Put your Tomb Raider disc image\n  (.cue with its .bin, .chd or .pbp)\n  in this folder:\n  %s\n  (no game data ships with this app)", DIRP);
        waitExit(msg); gfxExit(); return 0;
    }
    snprintf(sysDir, sizeof sysDir, "%s", DIRP);
    snprintf(SRM, sizeof SRM, "%s/tombraider.srm", DIRP);
    snprintf(STATE0, sizeof STATE0, "%s/tombraider.st1", DIRP);
    snprintf(STATE1, sizeof STATE1, "%s/tombraider.st2", DIRP);

    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);

    retro_set_environment(env_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);
    retro_init();
    logs("core initialised");

    struct retro_game_info gi; memset(&gi, 0, sizeof gi); gi.path = DISC;
    logp("loading %s", DISC);
    if (!retro_load_game(&gi)) { waitExit("  The core could not load the disc."); gfxExit(); return 0; }
    retro_get_system_av_info(&av);
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
    logp("loaded id %s, %.2f fps, %.0f Hz", CdromId, av.timing.fps, av.timing.sample_rate);
    /* the memory card */
    uint8_t *card = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    static uint8_t cardCopy[MCD_BYTES];
    if (card && retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) == MCD_BYTES) {
        FILE *f = fopen(SRM, "rb"); size_t n = 0;
        if (f) { n = fread(cardCopy, 1, MCD_BYTES, f); fclose(f); }
        if (n == MCD_BYTES && cardCopy[0] == 'M' && cardCopy[1] == 'C') memcpy(card, cardCopy, MCD_BYTES);
        else if (!(card[0] == 'M' && card[1] == 'C')) formatCard(card);
        memcpy(cardCopy, card, MCD_BYTES);
    } else card = NULL;
    /* the game runs a frame per refresh of the 3DS's slightly slower screen, so its sound is played that much slower too (0.3%,
       too little to hear) or the DSP would run out every few seconds */
    { double fps = av.timing.fps > 1 ? av.timing.fps : 60.0, rate = av.timing.sample_rate > 1 ? av.timing.sample_rate : 44100;
      soundInit(rate * SCREEN_HZ / fps); }
    uint8_t *ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    link_init(DIRP);

    static uint16_t link[LINK_W * LINK_H];
    LinkIn in; memset(&in, 0, sizeof in);
    in.ram = ram; in.n3ds = n3ds; in.dynarec = __ctr_svchax;
    static char msgbuf[40], st0[32], st1[32];
    in.state[0] = st0; in.state[1] = st1;
#define HAS(p) ({ FILE *t_ = fopen(p, "rb"); int r_ = t_ != NULL; if (t_) fclose(t_); r_; })
    snprintf(st0, sizeof st0, "%s", HAS(STATE0) ? "saved" : "empty"); snprintf(st1, sizeof st1, "%s", HAS(STATE1) ? "saved" : "empty");

    const u64 FRAME = (u64)(SYSCLOCK_ARM11 / SCREEN_HZ);
    struct { u64 emu, top, link, max, draw, copy; int n, ndraw; } tm; memset(&tm, 0, sizeof tm);
    u64 secAt = osGetTime(); int frames = 0, frame = 0, drawn = 0, quit = 0, linkDirty = 1;
    int touchWas = 0;
    const char *msgNow = NULL; int msgT = 0;

    while (!quit && aptMainLoop()) {
        u64 t0 = svcGetSystemTick();
        hidScanInput();
        u32 held = hidKeysHeld();
        unsigned pad = 0;
        if (held & KEY_A) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_A);
        if (held & KEY_B) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_B);
        if (held & KEY_X) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_X);
        if (held & KEY_Y) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_Y);
        if (held & KEY_L) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_L);
        if (held & KEY_R) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_R);
        if (held & KEY_ZL) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_L2);
        if (held & KEY_ZR) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_R2);
        if (held & KEY_START) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_START);
        if (held & KEY_SELECT) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_SELECT);
        /* the D-pad, and the Circle Pad as one (KEY_CPAD_* are libctru's own threshold) */
        if (held & (KEY_DUP | KEY_CPAD_UP)) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_UP);
        if (held & (KEY_DDOWN | KEY_CPAD_DOWN)) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_DOWN);
        if (held & (KEY_DLEFT | KEY_CPAD_LEFT)) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_LEFT);
        if (held & (KEY_DRIGHT | KEY_CPAD_RIGHT)) pad |= BTN(RETRO_DEVICE_ID_JOYPAD_RIGHT);
        int act = LINK_NONE;
        if (held & KEY_TOUCH) {
            touchPosition t; hidTouchRead(&t);
            act = link_touch(touchWas ? TOUCH_HELD : TOUCH_DOWN, t.px, t.py, &in);
            if (!touchWas || frame % 3 == 0) linkDirty = 1;          /* while dragging the map: 20 redraws a second */
            touchWas = 1;
        } else if (touchWas) { touchWas = 0; act = link_touch(TOUCH_UP, 0, 0, &in); linkDirty = 1; }
#ifdef CTR_TEST
        {   /* test build only (test/build_test.sh): buttons and taps by frame, because an emulator's input can't be driven headless.
               { frame, frames, pad bits, tap x, tap y } */
            static const struct { int at, len; unsigned pad; int tx, ty; } S[] = { TEST_SCRIPT };
            for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
                if (frame < S[i].at || frame >= S[i].at + S[i].len) continue;
                pad |= S[i].pad;
                if (S[i].tx >= 0 && frame == S[i].at) { link_touch(TOUCH_DOWN, S[i].tx, S[i].ty, &in); act = link_touch(TOUCH_UP, S[i].tx, S[i].ty, &in); linkDirty = 1; }
            }
# ifdef TEST_STATE
            if (frame == 2) logp("test state %s: %d", TEST_STATE, loadState(TEST_STATE));
# endif
# ifdef TEST_QUIT
            if (frame == TEST_QUIT) quit = 1;
# endif
        }
#endif
        padBits = pad;
        if (act == LINK_QUIT) quit = 1;
        else if (act == LINK_SAVE0 || act == LINK_SAVE1) {
            int ok = saveState(act == LINK_SAVE0 ? STATE0 : STATE1), s = act == LINK_SAVE1;
            snprintf(msgbuf, sizeof msgbuf, ok ? "SAVED TO SLOT %d" : "COULD NOT SAVE SLOT %d", s + 1); msgNow = msgbuf; msgT = 240;
            if (ok) snprintf(s ? st1 : st0, 32, "saved");
        } else if (act == LINK_LOAD0 || act == LINK_LOAD1) {
            int ok = loadState(act == LINK_LOAD0 ? STATE0 : STATE1), s = act == LINK_LOAD1;
            snprintf(msgbuf, sizeof msgbuf, ok ? "LOADED SLOT %d" : "COULD NOT LOAD SLOT %d", s + 1); msgNow = msgbuf; msgT = 240;
        } else if (act == LINK_FRAMESKIP) { frameskip = (frameskip + 1) % 3; varsDirty = 1; in.frameskip = frameskip; }
        else if (act == LINK_OLD3DS) { in.old3ds = !in.old3ds; osSetSpeedupEnable(!in.old3ds); }

        McdFlag[0] &= ~8; McdFlag[1] &= ~8;                        /* the memory card with the built-in BIOS, as psxlink.c */
        freshFrame = 0;
        retro_run();                                               /* video_cb draws into the top screen's back buffer */
        frames++;
        u64 t1 = svcGetSystemTick();
        if (freshFrame) { gfxFlushBuffers(); gfxSwapBuffers(); drawn++; }
        u64 t2 = svcGetSystemTick();

        frame++;
        if (msgT && --msgT == 0) msgNow = NULL;
        in.msg = msgNow;
        in.nowMs = (uint32_t)osGetTime();
        /* The Link screen: 10 times a second, or after a touch. A redraw due on a frame that has already used most of its time waits
           for a quieter one (half a second at most), so the map never costs the game a frame it could have had. */
        static int linkOwed;
        if (frame % link_interval() == 0 || linkDirty) linkOwed = linkOwed ? linkOwed : 1;
        int linkNow = linkOwed && (svcGetSystemTick() - t0 < FRAME * 6 / 10 || linkOwed > 30 || linkDirty);
        if (linkOwed && !linkNow) linkOwed++;
        if (linkNow) {
            linkOwed = 0;
            static uint16_t shown[LINK_W * LINK_H];
            u64 d0 = svcGetSystemTick();
            link_draw(link, &in);
            tm.draw += svcGetSystemTick() - d0; tm.ndraw++;
            linkDirty = 0;
            u64 c0 = svcGetSystemTick();
            if (memcmp(link, shown, sizeof link)) {
                memcpy(shown, link, sizeof link);
                uint16_t *fb = (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
                for (int x = 0; x < LINK_W; x++) {
                    uint16_t *dst = fb + x * 240 + 239; const uint16_t *s = link + x;
                    for (int y = 0; y < LINK_H; y++) { dst[-y] = *s; s += LINK_W; }
                }
                GSPGPU_FlushDataCache(fb, LINK_W * LINK_H * 2);   /* only the bottom screen's buffer */
            }
            tm.copy += svcGetSystemTick() - c0;
#ifdef TEST_SHOT
            /* test build: the screens as files at chosen frames, since an emulator's window can't be caught on an exact frame */
            { static const int SHOTS[] = { TEST_SHOT }; for (size_t i = 0; i < sizeof SHOTS / sizeof SHOTS[0]; i++) if (frame >= SHOTS[i] && frame < SHOTS[i] + link_interval()) {
                char p[340]; snprintf(p, sizeof p, "%s/shot_%06d_link.raw", DIRP, SHOTS[i]); saveFile(p, link, sizeof link);
                snprintf(p, sizeof p, "%s/shot_%06d_top.raw", DIRP, SHOTS[i]); saveFile(p, gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 400 * 240 * 2);
                char st[200]; link_status(st, sizeof st); logp("shot %d: %s", SHOTS[i], st);
            } }
#endif
        }
        if (frame % 600 == 0) link_flush();                        /* the Link settings and story progress, when they changed */
        if (card && frame % 300 == 0 && memcmp(card, cardCopy, MCD_BYTES)) {   /* the memory card: written when it changed */
            memcpy(cardCopy, card, MCD_BYTES);
            logp("card written: %d", saveFile(SRM, cardCopy, MCD_BYTES));
        }
        u64 t3 = svcGetSystemTick();
        tm.emu += t1 - t0; tm.top += t2 - t1; tm.link += t3 - t2; tm.n++;
        if (t3 - t0 > tm.max) tm.max = t3 - t0;
        if (osGetTime() - secAt >= 1000) {
            const double tenth = SYSCLOCK_ARM11 / 10000.0;
            in.fps = frames; in.drawn = drawn; frames = drawn = 0; secAt = osGetTime();
            in.msEmu = (int)(tm.emu / tenth / tm.n + 0.5);
            in.msTop = (int)(tm.top / tenth / tm.n + 0.5);
            in.msLink = (int)(tm.link / tenth / tm.n + 0.5);
            in.msMax = (int)(tm.max / tenth + 0.5);
            static int logTick;
            if (++logTick % 5 == 0) {                              /* the numbers to read back after a run on the real machine */
                char st[200]; link_status(st, sizeof st);
                int dr = tm.ndraw ? (int)(tm.draw / tenth / tm.ndraw + 0.5) : 0, cp = tm.ndraw ? (int)(tm.copy / tenth / tm.ndraw + 0.5) : 0;
                logp("f%d fps %d/%d emu %d.%d top %d.%d link %d.%d (%d draws of %d.%d, copy %d.%d) max %d.%d %s", frame, in.fps, in.drawn,
                     in.msEmu / 10, in.msEmu % 10, in.msTop / 10, in.msTop % 10, in.msLink / 10, in.msLink % 10, tm.ndraw, dr / 10, dr % 10, cp / 10, cp % 10, in.msMax / 10, in.msMax % 10, st);
            }
            memset(&tm, 0, sizeof tm);
        }
        /* Only a loop that came in under a refresh waits for the next one. A locked loop would drop to 30 the moment a frame runs a
           little long (Shantae's lesson), where this one just runs a little slow. */
        if (svcGetSystemTick() - t0 < FRAME) gspWaitForVBlank();
    }
    if (card && memcmp(card, cardCopy, MCD_BYTES)) saveFile(SRM, card, MCD_BYTES);
    link_flush();
    logp("quit at frame %d", frame);
    retro_unload_game();
    retro_deinit();
    if (haveSound) ndspExit();
    if (logf) fclose(logf);
    cfguExit();
    gfxExit();
    return 0;
}

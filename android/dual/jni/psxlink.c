/*
 * PSX Link native frontend: a small libretro host for the bundled
 * pcsx_rearmed core. Java drives the frame loop; this file owns the
 * libretro callbacks, the frame/audio hand-off buffers, input state,
 * memory card + save state files, disc swapping and RAM access for the
 * Link companion.
 */
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include "libretro.h"

extern int __android_log_print(int prio, const char *tag, const char *fmt, ...);
extern int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap);
#define LOGI(...) __android_log_print(4, "XenoLink", __VA_ARGS__)
#define LOGE(...) __android_log_print(6, "XenoLink", __VA_ARGS__)

extern char CdromId[10];
extern char CdromLabel[33];
extern int padToggleAnalog(unsigned int index);
/* the core's CPU interface (exported for us): Clear() tells the recompiler that RAM it may have compiled changed */
typedef struct { int (*Init)(); void (*Reset)(); void (*Execute)(void *); void (*ExecuteBlock)(void *, int);
                 void (*Clear)(uint32_t addr, uint32_t words); } core_cpu_t;
extern core_cpu_t *psxCpu;
/* The core marks memory cards "new" at power-on, reset and every state load. The built-in BIOS then answers the
 * game's card check with "new card" until something is written, and Xenogears' save menu takes that as
 * "not formatted" (loading still works). The card in the slot never changes behind the game's back here,
 * so the flag is cleared after every frame. */
extern unsigned char McdFlag[2];
static int g_xga_pad;
static int g_cheats_set = 0;
static volatile int g_xga_state;

/* ---------- paths / options ---------- */
static char g_sys_dir[512] = "", g_save_dir[512] = "", g_srm_path[600] = "";
#define MAX_VARS 64
static char g_var_key[MAX_VARS][64], g_var_val[MAX_VARS][64];
static int g_var_count = 0, g_var_dirty = 0;
static pthread_mutex_t g_var_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- video ---------- */
#define FB_MAX (2048 * 1024)
static uint16_t g_fb[FB_MAX];          /* latest frame, RGB565, tightly packed */
static int g_fb_w = 0, g_fb_h = 0, g_fb_seq = 0;
static pthread_mutex_t g_fb_lock = PTHREAD_MUTEX_INITIALIZER;
static enum retro_pixel_format g_pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;
static struct retro_system_av_info g_av;

/* ---------- audio ring (stereo int16 frames) ---------- */
#define AR_FRAMES 16384
static int16_t g_ar[AR_FRAMES * 2];
static volatile int g_ar_r = 0, g_ar_w = 0;
static pthread_mutex_t g_ar_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- input ---------- */
static volatile uint32_t g_btn[2];          /* retro joypad bitmask per port */
static volatile int16_t g_axis[2][4];       /* lx, ly, rx, ry */
static volatile int g_stick_dpad = 0;       /* left stick also drives the d-pad */
static volatile int g_stick_dpad_thresh = 12000;

/* ---------- misc state ---------- */
static struct retro_disk_control_ext_callback g_disk;
static int g_disk_ext = 0, g_disk_have = 0;
static int g_loaded = 0, g_inited = 0;
static char g_msg[256];
static volatile int g_msg_seq = 0;

/* ================= libretro callbacks ================= */
/* the core's last log lines, for the diagnostics report */
#define LOGRING 48
static char g_logring[LOGRING][160];
static int g_loghead = 0;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap; char line[160]; size_t n;
   va_start(ap, fmt);
   __android_log_vprint(level >= RETRO_LOG_ERROR ? 6 : level >= RETRO_LOG_WARN ? 5 : 4, "pcsx", fmt, ap);
   va_end(ap);
   va_start(ap, fmt); vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
   n = strlen(line); while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
   if (!n) return;
   pthread_mutex_lock(&g_log_lock);
   snprintf(g_logring[g_loghead % LOGRING], 160, "%s", line); g_loghead++;
   pthread_mutex_unlock(&g_log_lock);
}

static const char *var_get(const char *key)
{
   int i;
   for (i = 0; i < g_var_count; i++)
      if (!strcmp(g_var_key[i], key)) return g_var_val[i];
   return NULL;
}

static bool env_cb(unsigned cmd, void *data)
{
   switch (cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL) {
   case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char **)data = g_sys_dir; return true;
   case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:   *(const char **)data = g_save_dir; return true;
   case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      enum retro_pixel_format f = *(const enum retro_pixel_format *)data;
      if (f == RETRO_PIXEL_FORMAT_RGB565 || f == RETRO_PIXEL_FORMAT_XRGB8888 || f == RETRO_PIXEL_FORMAT_0RGB1555) { g_pixfmt = f; return true; }
      return false;
   }
   case RETRO_ENVIRONMENT_GET_VARIABLE: {
      struct retro_variable *v = data; const char *val;
      pthread_mutex_lock(&g_var_lock);
      val = var_get(v->key);
      v->value = val;
      pthread_mutex_unlock(&g_var_lock);
      return val != NULL;
   }
   case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
      *(bool *)data = g_var_dirty; g_var_dirty = 0; return true;
   case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      ((struct retro_log_callback *)data)->log = log_cb; return true;
   case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
   case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
   case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
      memcpy(&g_disk, data, sizeof(g_disk)); g_disk_ext = 1; g_disk_have = 1; return true;
   case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
      if (!g_disk_ext) { memset(&g_disk, 0, sizeof(g_disk)); memcpy(&g_disk, data, sizeof(struct retro_disk_control_callback)); g_disk_have = 1; }
      return true;
   case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: *(unsigned *)data = 1; return true;
   case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: memcpy(&g_av, data, sizeof(g_av)); return true;
   case RETRO_ENVIRONMENT_SET_GEOMETRY: memcpy(&g_av.geometry, data, sizeof(g_av.geometry)); return true;
   case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: *(unsigned *)data = 1; return true;
   case RETRO_ENVIRONMENT_SET_MESSAGE: {
      const struct retro_message *m = data;
      if (m && m->msg) { snprintf(g_msg, sizeof(g_msg), "%s", m->msg); g_msg_seq++; }
      return true;
   }
   case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
      const struct retro_message_ext *m = data;
      if (m && m->msg && m->target != RETRO_MESSAGE_TARGET_LOG) { snprintf(g_msg, sizeof(g_msg), "%s", m->msg); g_msg_seq++; }
      return true;
   }
   case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)data = 0; return true;
   case RETRO_ENVIRONMENT_SET_VARIABLES:
   case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
   case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
   case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
   case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
   case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
   case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      return true;
   }
   return false;
}

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch)
{
   unsigned x, y;
   if (!data || w == 0 || h == 0) return;                /* dupe: keep last frame */
   if ((size_t)w * h > FB_MAX) return;
   pthread_mutex_lock(&g_fb_lock);
   if (g_pixfmt == RETRO_PIXEL_FORMAT_RGB565) {
      for (y = 0; y < h; y++)
         memcpy(g_fb + y * w, (const uint8_t *)data + y * pitch, w * 2);
   } else if (g_pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) {
      for (y = 0; y < h; y++) {
         const uint32_t *s = (const uint32_t *)((const uint8_t *)data + y * pitch);
         uint16_t *d = g_fb + y * w;
         for (x = 0; x < w; x++) { uint32_t p = s[x]; d[x] = ((p >> 8) & 0xF800) | ((p >> 5) & 0x07E0) | ((p >> 3) & 0x1F); }
      }
   } else { /* 0RGB1555 */
      for (y = 0; y < h; y++) {
         const uint16_t *s = (const uint16_t *)((const uint8_t *)data + y * pitch);
         uint16_t *d = g_fb + y * w;
         for (x = 0; x < w; x++) { uint16_t p = s[x]; d[x] = ((p & 0x7FE0) << 1) | (p & 0x1F); }
      }
   }
   g_fb_w = w; g_fb_h = h; g_fb_seq++;
   pthread_mutex_unlock(&g_fb_lock);
}

static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
   size_t i;
   pthread_mutex_lock(&g_ar_lock);
   for (i = 0; i < frames; i++) {
      int nw = (g_ar_w + 1) % AR_FRAMES;
      if (nw == g_ar_r) g_ar_r = (g_ar_r + 1) % AR_FRAMES;    /* overrun: drop oldest */
      g_ar[g_ar_w * 2] = data[i * 2]; g_ar[g_ar_w * 2 + 1] = data[i * 2 + 1];
      g_ar_w = nw;
   }
   pthread_mutex_unlock(&g_ar_lock);
   return frames;
}
static void audio_cb(int16_t l, int16_t r) { int16_t s[2] = { l, r }; audio_batch_cb(s, 1); }

static void input_poll_cb(void) {}

/* Set by tra_tick (analog controls): while Lara is under the player's control the sticks are read there, not as a D-pad */
static int g_tra_play = 0;
static uint32_t g_tra_mask = 0;
static uint32_t g_trd_mask = 0;   /* dev codes: Cross pressed through a warp's stats screen */

static uint32_t port_mask(unsigned port)
{
   uint32_t m = g_btn[port];
   if (port == 0) m |= g_trd_mask;
   if (port == 0 && g_tra_play) return m | g_tra_mask;
   if (g_stick_dpad) {
      int t = g_stick_dpad_thresh;
      if (g_axis[port][0] < -t) m |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
      if (g_axis[port][0] >  t) m |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
      if (g_axis[port][1] < -t) m |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
      if (g_axis[port][1] >  t) m |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   }
   return m;
}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port > 1) return 0;
   switch (device & RETRO_DEVICE_MASK) {
   case RETRO_DEVICE_JOYPAD:
      if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)port_mask(port);
      return (port_mask(port) >> id) & 1;
   case RETRO_DEVICE_ANALOG:
      if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) {
         if (id == RETRO_DEVICE_ID_JOYPAD_L2 || id == RETRO_DEVICE_ID_JOYPAD_R2)
            return ((port_mask(port) >> id) & 1) ? 0x7fff : 0;
         return 0;
      }
      if (index > 1 || id > 1) return 0;
      return g_axis[port][index * 2 + id];
   }
   return 0;
}

/* ================= helpers ================= */
#define MCD_BYTES 131072
static void format_card(uint8_t *d)
{
   unsigned off = 0, i;
   memset(d, 0, MCD_BYTES);
   d[off++] = 'M'; d[off++] = 'C'; off += 0x7d; d[off++] = 0x0e;
   for (i = 0; i < 15; i++) { d[off++] = 0xa0; off += 0x07; d[off++] = 0xff; d[off++] = 0xff; off += 0x75; d[off++] = 0xa0; }
   for (i = 0; i < 20; i++) { d[off++] = 0xff; d[off++] = 0xff; d[off++] = 0xff; d[off++] = 0xff; off += 0x04; d[off++] = 0xff; d[off++] = 0xff; off += 0x76; }
}
static int card_formatted(const uint8_t *d) { return d[0] == 'M' && d[1] == 'C'; }
static int g_card_status = 0;   /* after loadGame: 0 = card loaded, 1 = no file yet (blank card), 2 = file was not a formatted card (backed up, blank card used) */
static int load_file(const char *path, void *buf, size_t size)
{
   FILE *f = fopen(path, "rb"); size_t n;
   if (!f) return 0;
   n = fread(buf, 1, size, f); fclose(f);
   return n == size;
}
static int save_file(const char *path, const void *buf, size_t size)
{
   char tmp[700]; FILE *f;
   snprintf(tmp, sizeof(tmp), "%s.tmp", path);
   f = fopen(tmp, "wb");
   if (!f) return 0;
   if (fwrite(buf, 1, size, f) != size) { fclose(f); remove(tmp); return 0; }
   fclose(f);
   return rename(tmp, path) == 0;
}

static const char *jstr(JNIEnv *e, jstring s, char *out, size_t n)
{
   const char *c = (*e)->GetStringUTFChars(e, s, NULL);
   snprintf(out, n, "%s", c ? c : "");
   if (c) (*e)->ReleaseStringUTFChars(e, s, c);
   return out;
}

#define J(name) Java_com_jack_tombraider_Native_##name

/* ================= JNI ================= */
JNIEXPORT void JNICALL J(init)(JNIEnv *e, jclass c, jstring sys, jstring save)
{
   jstr(e, sys, g_sys_dir, sizeof(g_sys_dir));
   jstr(e, save, g_save_dir, sizeof(g_save_dir));
   if (g_inited) return;
   retro_set_environment(env_cb);
   retro_set_video_refresh(video_cb);
   retro_set_audio_sample(audio_cb);
   retro_set_audio_sample_batch(audio_batch_cb);
   retro_set_input_poll(input_poll_cb);
   retro_set_input_state(input_state_cb);
   retro_init();
   g_inited = 1;
   LOGI("core initialised, sys=%s save=%s", g_sys_dir, g_save_dir);
}

JNIEXPORT void JNICALL J(setVariable)(JNIEnv *e, jclass c, jstring k, jstring v)
{
   char key[64], val[64]; int i;
   jstr(e, k, key, sizeof(key)); jstr(e, v, val, sizeof(val));
   pthread_mutex_lock(&g_var_lock);
   for (i = 0; i < g_var_count; i++) if (!strcmp(g_var_key[i], key)) break;
   if (i == g_var_count && g_var_count < MAX_VARS) g_var_count++;
   if (i < MAX_VARS) { strcpy(g_var_key[i], key); strcpy(g_var_val[i], val); g_var_dirty = 1; }
   pthread_mutex_unlock(&g_var_lock);
}

JNIEXPORT jboolean JNICALL J(loadGame)(JNIEnv *e, jclass c, jstring jpath, jstring jsrm, jint padType)
{
   char path[600]; struct retro_game_info gi; void *sram; size_t sz;
   jstr(e, jpath, path, sizeof(path));
   jstr(e, jsrm, g_srm_path, sizeof(g_srm_path));
   memset(&gi, 0, sizeof(gi)); gi.path = path;
   if (!retro_load_game(&gi)) { LOGE("load failed: %s", path); return JNI_FALSE; }
   retro_get_system_av_info(&g_av);
   /* padType: 0 = standard, 1 = analog (dual analog), 2 = DualShock */
   {
      unsigned dev = padType == 2 ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
                   : padType == 1 ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)
                   : RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0);
      retro_set_controller_port_device(0, dev);
      retro_set_controller_port_device(1, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0));
   }
   sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   sz = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   g_card_status = 1;
   if (sram && sz == MCD_BYTES && g_srm_path[0]) {
      static uint8_t tmp[MCD_BYTES];
      FILE *f = fopen(g_srm_path, "rb");
      if (f) {
         size_t n = fread(tmp, 1, MCD_BYTES, f); fclose(f);
         if (n == MCD_BYTES && card_formatted(tmp)) { memcpy(sram, tmp, MCD_BYTES); g_card_status = 0; LOGI("memory card loaded: %s", g_srm_path); }
         else {
            /* blank, cut short or not a card: keep it aside and start from a formatted card */
            char bad[700]; snprintf(bad, sizeof(bad), "%s.bad-%ld", g_srm_path, (long)time(NULL));
            rename(g_srm_path, bad);
            format_card(sram);
            save_file(g_srm_path, sram, MCD_BYTES);
            g_card_status = 2;
            LOGE("memory card %s was not a formatted card (%zu bytes); kept as %s, using a new formatted card", g_srm_path, n, bad);
         }
      } else if (!card_formatted(sram)) format_card(sram);
   }
   g_loaded = 1;
   g_xga_pad = 0; g_xga_state = 0;
   LOGI("loaded %s id=%s fps=%f", path, CdromId, g_av.timing.fps);
   return JNI_TRUE;
}

JNIEXPORT void JNICALL J(setPadType)(JNIEnv *e, jclass c, jint padType)
{
   unsigned dev = padType == 2 ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
                : padType == 1 ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)
                : RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0);
   if (g_loaded) retro_set_controller_port_device(0, dev);
   g_xga_pad = 0;                         /* a new pad starts digital: the analog installer switches it again */
}

/* ---- Analog controls (Jack's xg_analog program), installed by the frontend ----
 * The program and hook are the same as in "Xenogears (USA) - Analog Controls.cht", but instead of the cheat
 * engine rewriting 2.3 KB of RAM every frame from power-on, this writes them only once the Xenogears
 * executable is in RAM, only into a block that is still empty (or already holds the program), tells the
 * recompiler about the write, and takes the hook out again if the game ever puts something else there. */
#define XGA_ADDR   0x17400u                 /* program, 0x80017400 */
#define XGA_SLOT   0x501FCu                 /* game's per-vblank hook pointer, 0x800501FC */
#define XGA_OPTS   0x17F04u                 /* option countdown bytes 0x80017F04..08 */
static uint8_t g_xga_prog[0xC00];
static int g_xga_len = 0;
static volatile int g_xga_on = 0, g_xga_opts = 0;   /* g_xga_state: 0 off, 1 waiting, 2 running, 3 blocked */
/* g_xga_pad: analog mode already switched on for this run */
static const uint8_t XGA_SIG[8] = { 0x05,0x80,0x02,0x3c, 0xfc,0x01,0x42,0x8c };   /* lui v0,0x8005; lw v0,0x1fc(v0) at 0x80036388 */

static void xga_set_analog(int on)
{
   int r = padToggleAnalog(0);
   if ((r != 0) != (on != 0)) padToggleAnalog(0);
}

static void xga_tick(void)
{
   uint8_t *ram = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   uint32_t slot, mine = 0x80000000u | XGA_ADDR;
   int i;
   if (!ram || !g_xga_len) return;
   memcpy(&slot, ram + XGA_SLOT, 4);
   if (!g_xga_on) {
      if (slot == mine) { slot = 0; memcpy(ram + XGA_SLOT, &slot, 4); }
      g_xga_state = 0; return;
   }
   if (memcmp(ram + 0x36388, XGA_SIG, 8)) { g_xga_state = 1; return; }      /* Xenogears isn't running (yet) */
   if (memcmp(ram + XGA_ADDR, g_xga_prog, g_xga_len)) {
      for (i = 0; i < g_xga_len; i++) if (ram[XGA_ADDR + i]) break;
      /* another copy of the same program (e.g. the .cht entered as a mod) starts with the same code: replace it */
      if (i < g_xga_len && g_xga_len >= 16 && !memcmp(ram + XGA_ADDR, g_xga_prog, 16)) i = g_xga_len;
      if (i < g_xga_len) {                                                    /* the game is using that block: stay out */
         if (slot == mine) { slot = 0; memcpy(ram + XGA_SLOT, &slot, 4); }
         if (g_xga_state != 3) LOGI("analog controls: 0x80017400 holds game data, hook removed");
         g_xga_state = 3; return;
      }
      memcpy(ram + XGA_ADDR, g_xga_prog, g_xga_len);
      if (psxCpu && psxCpu->Clear) psxCpu->Clear(0x80000000u | XGA_ADDR, (g_xga_len + 3) / 4);
   }
   for (i = 0; i < 5; i++) if (g_xga_opts & (1 << i)) ram[XGA_OPTS + i] = 0x10;
   if (slot == 0) { memcpy(ram + XGA_SLOT, &mine, 4); }
   if (!g_xga_pad) { xga_set_analog(1); g_xga_pad = 1; }
   if (g_xga_state != 2) LOGI("analog controls running");
   g_xga_state = 2;
}

/* prog = the program's bytes from 0x80017400; opts = bit per option (walk, run, camera, invert, smooth) */
JNIEXPORT void JNICALL J(setAnalogMod)(JNIEnv *e, jclass c, jbyteArray prog, jboolean on, jint opts)
{
   if (prog) {
      jsize n = (*e)->GetArrayLength(e, prog);
      if (n > (jsize)sizeof(g_xga_prog)) n = sizeof(g_xga_prog);
      (*e)->GetByteArrayRegion(e, prog, 0, n, (jbyte *)g_xga_prog);
      g_xga_len = n;
      /* Fix: the camera check read the field's camera mode (a 16-bit value at 0x800AF934) with lw, which also took in
       * the next halfword (0x800AF936, e.g. 0x0019 in Fei's room), so the right stick and smooth L1/R1 never ran
       * there. lw t5,-0x6cc(t2) -> lhu t5,-0x6cc(t2). */
      if (n >= 0x460) {
         uint32_t w; memcpy(&w, g_xga_prog + 0x45C, 4);
         if (w == 0x8D4DF934u) { w = 0x954DF934u; memcpy(g_xga_prog + 0x45C, &w, 4); }
      }
   }
   if (g_xga_on && !on && g_loaded && g_xga_pad) { xga_set_analog(0); }
   if (!on) g_xga_pad = 0;
   g_xga_on = on; g_xga_opts = opts;
   if (g_loaded) xga_tick();
}
JNIEXPORT jint JNICALL J(analogModState)(JNIEnv *e, jclass c) { return g_xga_state; }

/* ---- Analog controls for Tomb Raider (Mods screen) ----
 * Nothing is put into the game's code and the pad stays digital. Before each frame this looks at the sticks and the game's RAM, and
 * then presses the game's own buttons for it (g_tra_mask) and writes Lara's heading and the camera's two aim fields, which the game
 * reads anyway. It acts only while the level clock runs, so the inventory, menus and cut scenes get the stick as a D-pad as usual.
 * Addresses: docs/RAM_map.md. Lara's states are the game's animation states, item +0x0E. */
#define TRA_MOVE 1      /* Lara runs the way the stick points, from the camera's view (off: left stick = D-pad) */
#define TRA_WALK 2      /* a light push walks */
#define TRA_CAM  4      /* right stick turns the camera */
#define TRA_INV  8      /* up and down on the right stick swapped */
#define TRA_FREE 16     /* Detached camera: only the right stick turns it, it never follows her round */
#define TRA_NOLEFT 32   /* Detached camera alone, Analog controls off: the left stick isn't ours */
static volatile int g_tra_on = 0, g_tra_opts = 0;
static uint32_t g_tra_clock = 0;
static int g_tra_clock_age = 1000, g_tra_level = -1, g_tra_cam = 0, g_tra_idle = 0, g_tra_yaw = 0, g_tra_elev = 0, g_tra_held = 0, g_tra_off = 0, g_tra_stick = 0, g_tra_walking = 0, g_tra_vx = 0, g_tra_vy = 0;

static uint32_t tra_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int32_t tra_s32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int tra_s16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static void tra_w16(uint8_t *p, int v) { int16_t s = (int16_t)v; memcpy(p, &s, 2); }
static int tra_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x80200000u; }
/* the same angle units as the game (65536 = a full turn, 0 = +z, growing to the right) */
static int tra_angle(int x, int z)      /* atan2 within a quarter degree (the Android toolchain here links no libm) */
{
   double ax = abs(x), az = abs(z), r; int a;
   if (!x && !z) return 0;
   r = ax < az ? ax / az : az / ax;
   a = (int)(r * (0.7854 + 0.273 * (1.0 - r)) * 32768.0 / 3.14159265);  /* 0..1/8 turn */
   if (ax > az) a = 0x4000 - a;                                         /* from +z towards +x */
   if (z < 0) a = 0x8000 - a;
   return x < 0 ? -a : a;
}
static int tra_len(int x, int y) { int a = abs(x), b = abs(y); return a > b ? a + 3 * b / 8 : b + 3 * a / 8; }   /* within 7% */
static int tra_clamp(int v, int m) { return v > m ? m : v < -m ? -m : v; }

/* The release, as the page's REVS: names table where Rev 6 keeps it, or v1.0's. Returns 0 if neither (not this game, or not loaded yet). */
static int tra_rev(const uint8_t *ram, int *d, int *b)
{
   uint32_t p = tra_u32(ram + 0x87710), q = tra_u32(ram + 0x87714);
   if (tra_ptr(p) && tra_ptr(q) && p - q == 8) { *d = 0; *b = 0; return 1; }
   p = tra_u32(ram + 0x87628); q = tra_u32(ram + 0x8762C);
   if (tra_ptr(p) && tra_ptr(q) && p - q == 8) { *d = -0xE8; *b = -0x100; return 1; }
   return 0;
}

static void tra_cam_start(int cam_heading)
{
   g_tra_cam = 1; g_tra_yaw = cam_heading; g_tra_elev = 0; g_tra_vx = g_tra_vy = 0; g_tra_idle = 1000; g_tra_off = 0;
}

/* The game's camera swings straight round behind Lara, which is what makes a stick that means "this way on the screen" impossible: the
 * screen keeps turning under it, and holding down would turn her round for ever. So while she's on her feet or jumping (own) the mod holds
 * the camera's heading itself, as a world direction. It only follows her slowly, by how far sideways she's running: straight away from or
 * towards the camera it doesn't move at all, and holding right runs her in a wide circle. In water, on ledges and so on the game has its camera. */
static void tra_camera(uint8_t *cam, int yrot, int moving, int cam_heading, int own, int running, int far)
{
   int free = g_tra_opts & TRA_FREE, rx = g_axis[0][2], ry = g_axis[0][3], push = (g_tra_opts & (TRA_CAM | TRA_FREE)) ? tra_len(rx, ry) : 0, wx = 0, wy = 0;
   if (push > 7000) {
      if (!g_tra_cam) tra_cam_start(cam_heading);
      g_tra_idle = 0;
      /* speed grows with the square of the push, so a small push is a fine adjustment */
      wx = rx * abs(rx) / 32767 * 0x1C0 / 32767;
      wy = ((g_tra_opts & TRA_INV) ? ry : -ry) * abs(ry) / 32767 * 0xD0 / 32767;     /* stick up looks up */
   }
   /* it speeds up and slows down over a few frames instead of starting and stopping dead */
   g_tra_vx += (wx - g_tra_vx) / 4 + (wx > g_tra_vx) - (wx < g_tra_vx);
   g_tra_vy += (wy - g_tra_vy) / 4 + (wy > g_tra_vy) - (wy < g_tra_vy);
   if (g_tra_cam) { g_tra_yaw += g_tra_vx; g_tra_elev = tra_clamp(g_tra_elev + g_tra_vy, 0x3000); }
   if (push <= 7000 && g_tra_idle < 1000) g_tra_idle++;
   if (own || free) {
      if (!free && running && moving && g_tra_idle > 90) {
         int rel = (int16_t)(yrot - g_tra_yaw), side = abs(rel) > 0x4000 ? (rel > 0 ? 0x8000 - rel : -0x8000 - rel) : rel;
         g_tra_yaw += side / 128;                                       /* at most 0x80 a tick, 40 degrees a second */
      }
      /* a fixed camera, a fight or a wall has the picture somewhere else: go by what's on the screen */
      if (far && g_tra_idle > 30 && abs((int16_t)(cam_heading - g_tra_yaw)) > 0x2000) { if (++g_tra_off > 15) { g_tra_yaw = cam_heading; g_tra_off = 0; } }
      else g_tra_off = 0;
   } else if (push <= 7000 && g_tra_cam && g_tra_idle > 90 && moving) {
      /* left alone and Lara on the move: it drifts back behind her, then the game has its camera back */
      int d = (int16_t)(yrot - g_tra_yaw);
      g_tra_yaw += d / 16 + (d > 0) - (d < 0); g_tra_elev -= g_tra_elev / 16;
      if (abs(d) < 0x100 && abs(g_tra_elev) < 0x100) g_tra_cam = 0;
   }
   if (g_tra_cam && tra_s32(cam + 0x20) == 0) {          /* only the camera that follows her, type 0 */
      tra_w16(cam + 0x40, g_tra_yaw - yrot);             /* aim fields, relative to her heading: the game clears them every frame */
      tra_w16(cam + 0x44, g_tra_elev);
   }
}

static void tra_tick(void)
{
   uint8_t *ram, *L, *cam;
   uint32_t lp, clock, m = 0;
   int d, b, level, st, yrot, lx, ly, push, cam_heading, dx, dz, land, air, own;
   g_tra_play = 0; g_tra_mask = 0;
   if (!g_tra_on || !(ram = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)) || !tra_rev(ram, &d, &b)) return;
   level = tra_s16(ram + 0x87668 + d);
   if (level != g_tra_level) { g_tra_level = level; g_tra_cam = 0; }
   clock = tra_u32(ram + 0x928F8 + b);
   if (clock - g_tra_clock > 60u) g_tra_cam = 0;                        /* the clock jumped: a loaded state or save, the camera is somewhere new */
   if (clock != g_tra_clock) { g_tra_clock = clock; g_tra_clock_age = 0; } else if (g_tra_clock_age < 1000) g_tra_clock_age++;
   if (level < 0 || level > 15 || g_tra_clock_age > 8 || ram[0x87670 + d]) return;
   if (level != 0 && tra_s16(ram + 0x885BA + d) != -1) return;          /* a demo */
   lp = tra_u32(ram + 0x89FAC + b);
   if (!tra_ptr(lp) || (lp & 0x1FFFFF) > 0x200000 - 72) return;
   L = ram + (lp & 0x1FFFFF); cam = ram + 0x89C34 + b;
   if (tra_s16(L + 0x0C) != 0 || tra_s16(L + 0x22) <= 0) return;        /* not Lara, or she's dead */
   g_tra_play = !(g_tra_opts & TRA_NOLEFT);
   st = tra_s16(L + 0x0E); yrot = tra_s16(L + 0x3E);
   dx = tra_s32(cam + 0x10) - tra_s32(cam); dz = tra_s32(cam + 0x18) - tra_s32(cam + 8);
   cam_heading = abs(dx) + abs(dz) < 64 ? yrot : tra_angle(dx, dz);
   lx = g_axis[0][0]; ly = g_axis[0][1]; push = tra_len(lx, ly);
   land = st == 0 || st == 1 || st == 2 || st == 6 || st == 7 || st == 20;    /* walk, run, stand, the turns */
   air = st == 3 || st == 9 || st == 11;                                /* jumping forward, falling, reaching for a ledge */
   own = (g_tra_opts & TRA_MOVE) && (land || air || st == 15);          /* the mod holds the camera's heading in these, see tra_camera */
   if ((own || (g_tra_opts & TRA_FREE)) && !g_tra_cam) tra_cam_start(cam_heading);
   if (g_tra_play && push > 8000 && !(g_btn[0] & 0xF0)) {               /* the D-pad, if held, wins */
      if (own) {
         /* The stick's direction on the screen, always: the heading the camera holds (tra_camera) plus the stick's angle */
         int stick = tra_angle(lx, -ly), diff;
         if (!g_tra_held) g_tra_stick = stick; else g_tra_stick += (int16_t)(stick - g_tra_stick) / 2;     /* takes out the stick's own jitter */
         g_tra_stick = (int16_t)g_tra_stick;
         g_tra_held = 1;
         diff = (int16_t)(g_tra_yaw + g_tra_stick - yrot);
         if (land) {
            /* half the remaining angle each tick up to a limit, so a turn eases in rather than stopping dead: right round takes about
             * 0.15 s standing and 0.2 s on the move. Standing she sets off once nearly round, so she doesn't step off a ledge on the way */
            int moving = st == 0 || st == 1, turn = abs(diff) < 0x40 ? diff : tra_clamp(diff / 2, moving ? 0x1000 : 0x1800);
            tra_w16(L + 0x3E, yrot + turn);
            if (moving || abs(diff - turn) < 0x2000) m |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
         } else if (st == 15) m |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;     /* crouching to jump: forward, the way she's turned */
         else if (abs(diff) > 0x800) m |= 1u << (diff > 0 ? RETRO_DEVICE_ID_JOYPAD_RIGHT : RETRO_DEVICE_ID_JOYPAD_LEFT);
      } else {                                                          /* hanging, swimming, blocks and the rest: as the D-pad */
         if (lx < -9000) m |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
         if (lx >  9000) m |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
         if (ly < -9000) m |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
         if (ly >  9000) m |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
      }
      /* a gap between walking and running, so a push near the line doesn't flick between them */
      g_tra_walking = g_tra_walking ? push < 22000 : push < 17000;
      if ((g_tra_opts & TRA_WALK) && land && g_tra_walking) m |= 1u << RETRO_DEVICE_ID_JOYPAD_R;   /* a light push */
   } else { g_tra_held = 0; g_tra_walking = 1; }
   /* the camera last: its aim is relative to the heading just set, or it shakes while she turns */
   if (g_tra_opts & (TRA_CAM | TRA_MOVE | TRA_FREE)) tra_camera(cam, tra_s16(L + 0x3E), tra_s16(L + 0x1E) != 0, cam_heading, own, push > 8000, abs(dx) + abs(dz) > 800); else g_tra_cam = 0;
   g_tra_mask = m;
}

JNIEXPORT void JNICALL J(setTrAnalog)(JNIEnv *e, jclass c, jboolean on, jint opts)
{
   g_tra_on = on; g_tra_opts = opts;
   if (!on) { g_tra_play = 0; g_tra_mask = 0; g_tra_cam = 0; }
}

/* ---- Dev codes for Tomb Raider (hidden behind the Konami code, off by default) ----
 * Like the analog controls these only write the game's RAM between frames. Addresses: docs/RAM_map.md. */
#define TRD_GOD  1
#define TRD_AIR  2
#define TRD_AMMO 4
enum { TRD_HEAL = 1, TRD_WEAPONS, TRD_MEDI, TRD_KEYS };
#define TRD_CREDITS 99   /* g_trd_going while a warp to the credits goes through the end screens */
static volatile int g_trd_opts = 0, g_trd_do = 0, g_trd_warp = -1;
static uint32_t g_trd_clock = 0;
static int g_trd_age = 1000, g_trd_going = -1, g_trd_t = 0;

/* An inventory ring: its count, quantities (s16) and list of pointers into the game's item table, kept in the game's order (+0x38 of an entry) */
static uint32_t trd_entry(int d, int idx) { return 0x80087FACu + d + 0x40 * idx; }
static int trd_find(uint8_t *ram, int count, int list, uint32_t e)
{
   int i, n = tra_s16(ram + count);
   for (i = 0; i < n; i++) if (tra_u32(ram + list + 4 * i) == e) return i;
   return -1;
}
static void trd_add(uint8_t *ram, int d, int count, int qty, int list, int idx, int q)
{
   uint32_t e = trd_entry(d, idx);
   int n = tra_s16(ram + count), at = trd_find(ram, count, list, e), pos = tra_s16(ram + (e & 0x1FFFFF) + 0x38), i;
   if (at >= 0) { if (tra_s16(ram + qty + 2 * at) < q) tra_w16(ram + qty + 2 * at, q); return; }
   if (n < 0 || n >= 23) return;
   for (at = 0; at < n; at++) { uint32_t o = tra_u32(ram + list + 4 * at); if (tra_ptr(o) && tra_s16(ram + (o & 0x1FFFFF) + 0x38) > pos) break; }
   for (i = n; i > at; i--) { memcpy(ram + list + 4 * i, ram + list + 4 * (i - 1), 4); memcpy(ram + qty + 2 * i, ram + qty + 2 * (i - 1), 2); }
   memcpy(ram + list + 4 * at, &e, 4); tra_w16(ram + qty + 2 * at, q); tra_w16(ram + count, n + 1);
}
static void trd_remove(uint8_t *ram, int d, int count, int qty, int list, int idx)
{
   int n = tra_s16(ram + count), at = trd_find(ram, count, list, trd_entry(d, idx)), i;
   if (at < 0) return;
   for (i = at; i < n - 1; i++) { memcpy(ram + list + 4 * i, ram + list + 4 * (i + 1), 4); memcpy(ram + qty + 2 * i, ram + qty + 2 * (i + 1), 2); }
   tra_w16(ram + count, n - 1);
}
static void trd_w32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }

static void trd_action(uint8_t *ram, int d, int b, uint8_t *L, uint8_t *info, int what)
{
   int mc = 0x8872C + d, mq = 0x88734 + d, ml = 0x88764 + d, kc = 0x887C0 + d, kq = 0x887C8 + d, kl = 0x887F8 + d, i;
   if (tra_s16(ram + (trd_entry(d, 0) & 0x1FFFFF) + 4) != 72) return;      /* not the item table this code knows */
   switch (what) {
   case TRD_HEAL: tra_w16(L + 0x22, 1000); break;
   case TRD_WEAPONS:
      for (i = 1; i <= 4; i++) trd_add(ram, d, mc, mq, ml, i, 1);          /* pistols, shotgun, magnums, uzis */
      for (i = 6; i <= 8; i++) trd_remove(ram, d, mc, mq, ml, i);          /* ammo for a gun she didn't have: the gun carries it now */
      trd_w32(info + 0xA4, 1000); trd_w32(info + 0xB0, 2000); trd_w32(info + 0xBC, 1200);
      break;
   case TRD_MEDI: trd_add(ram, d, mc, mq, ml, 10, 20); trd_add(ram, d, mc, mq, ml, 11, 20); break;
   case TRD_KEYS: {
      /* only what this level has, from its pickups, keyholes and puzzle slots: the rest have no model or name loaded and sit invisible in the ring */
      uint32_t ip = tra_u32(ram + 0x89BFC + b);
      if (tra_ptr(ip) && (ip & 0x1FFFFF) <= 0x200000 - 256 * 72) for (i = 0; i < 256; i++) {
         int t = tra_s16(ram + (ip & 0x1FFFFF) + 72 * i + 0x0C), idx = -1;
         if (t >= 110 && t <= 113) idx = 16 + t - 110; else if (t >= 118 && t <= 121) idx = 16 + t - 118;
         else if (t >= 129 && t <= 132) idx = 20 + t - 129; else if (t >= 137 && t <= 140) idx = 20 + t - 137;
         if (idx >= 0) trd_add(ram, d, kc, kq, kl, idx, 1);
      }
      break;
   }
   }
}

static void trd_tick(void)
{
   uint8_t *ram, *L = NULL, *info = NULL;
   uint32_t clock, lp;
   int d, b, level, ending, play, hp;
   g_trd_mask = 0;
   if (!(ram = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)) || !tra_rev(ram, &d, &b)) return;
   level = tra_s16(ram + 0x87668 + d); ending = ram[0x87670 + d];
   clock = tra_u32(ram + 0x928F8 + b);
   if (clock != g_trd_clock) { g_trd_clock = clock; g_trd_age = 0; } else if (g_trd_age < 1000) g_trd_age++;
   lp = tra_u32(ram + 0x89FAC + b);
   play = level >= 0 && level <= 15 && !ending && g_trd_age <= 8 && (level == 0 || tra_s16(ram + 0x885BA + d) == -1) && tra_ptr(lp) && (lp & 0x1FFFFF) <= 0x200000 - 72;
   if (play) { L = ram + (lp & 0x1FFFFF); if (tra_s16(L + 0x0C) != 0) play = 0; }
   if (play) { uint32_t ip = tra_u32(L + 0x2C); if (tra_ptr(ip) && (ip & 0x1FFFFF) <= 0x200000 - 0x100) info = ram + (ip & 0x1FFFFF); else play = 0; }

   /* A level warp: the level is ended with the number before the one wanted, and the game goes on from there by itself. Levels that follow a cut
    * scene (5, 10, 14, 15) are asked for through its number instead, 16-19, so it doesn't play. Cross gets it past the stats screen and a movie.
    * The word at 0x885BA is 1 in Lara's Home, where leaving would otherwise make the next level run as a demo. */
   if (g_trd_going < 0 && g_trd_warp != -1 && play) {
      int w = g_trd_warp == -2 ? level + 1 : g_trd_warp == -3 ? level : g_trd_warp, from = w == 5 ? 16 : w == 10 ? 17 : w == 14 ? 18 : w == 15 ? 19 : w - 1;
      g_trd_warp = -1;
      if (w == -4) w = TRD_CREDITS, from = 15;                              /* the credits: the last level ended, so the game plays its ending */
      if ((w >= 1 && w <= 15) || w == TRD_CREDITS) {                        /* not Lara's Home, and nothing after the last level */
         g_trd_going = w; g_trd_t = 0;
         tra_w16(ram + 0x87668 + d, from); tra_w16(ram + 0x885BA + d, -1); ram[0x87670 + d] = 1;
         return;
      }
   }
   /* The ending: the Great Pyramid's stats screen, then "Overwrite game on the memory card", then the ending movie, THE END and the credits.
    * Down comes before every Cross, so the card question always gets "Exit without saving" (and "Are you sure?" gets "No"). The word at
    * 0x8744A is 1 in play and on those two screens and 0 once the movie is on its way (seen from Caves and St. Francis' Folly; 0x89DA0
    * looked right from Caves but is 1 in St. Francis' Folly): then the buttons stop, so the movie and credits play. */
   if (g_trd_going == TRD_CREDITS) {
      if (tra_s16(ram + 0x8744A + d) == 0 || level != 15 || !ending || ++g_trd_t > 18000) g_trd_going = -1;
      else {
         int m = g_trd_t & 31;
         if (m < 6) g_trd_mask = 1u << RETRO_DEVICE_ID_JOYPAD_DOWN; else if (m >= 16 && m < 22) g_trd_mask = 1u << RETRO_DEVICE_ID_JOYPAD_B;
         return;
      }
   }
   if (g_trd_going >= 0) {
      if ((level == g_trd_going && !ending && g_trd_age <= 8) || ++g_trd_t > 18000) g_trd_going = -1;   /* there, or five minutes: give up */
      else {
         if ((g_trd_t & 31) < 6) g_trd_mask = 1u << RETRO_DEVICE_ID_JOYPAD_B;   /* libretro's B is the PlayStation's Cross */
         return;
      }
   }
   if (!play) return;
   if (g_trd_do) { trd_action(ram, d, b, L, info, g_trd_do); g_trd_do = 0; }
   hp = tra_s16(L + 0x22);
   if (g_trd_opts & TRD_GOD) {
      if (hp < 1000) tra_w16(L + 0x22, 1000);
      if (tra_s16(L + 0x20) > 130) tra_w16(L + 0x20, 130);                  /* a fall never gets fast enough to kill */
   }
   if (g_trd_opts & TRD_AIR) tra_w16(info + 0x12, 1800);
   if (g_trd_opts & TRD_AMMO) {
      if (tra_s32(info + 0xA4) < 1000) trd_w32(info + 0xA4, 1000);
      if (tra_s32(info + 0xB0) < 2000) trd_w32(info + 0xB0, 2000);
      if (tra_s32(info + 0xBC) < 1200) trd_w32(info + 0xBC, 1200);
   }
}

JNIEXPORT void JNICALL J(setTrDev)(JNIEnv *e, jclass c, jint opts) { g_trd_opts = opts; }
JNIEXPORT void JNICALL J(trDevDo)(JNIEnv *e, jclass c, jint what) { g_trd_do = what; }
JNIEXPORT void JNICALL J(trWarp)(JNIEnv *e, jclass c, jint level) { g_trd_warp = level; }
/* The game's own level names, one to a line, from its name table */
JNIEXPORT jstring JNICALL J(trLevels)(JNIEnv *e, jclass c)
{
   static char out[800];
   size_t o = 0; int i, d, b;
   uint8_t *ram = g_loaded ? (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) : NULL;
   out[0] = 0;
   if (ram && tra_rev(ram, &d, &b)) for (i = 0; i < 16 && o < sizeof(out) - 50; i++) {
      uint32_t p = tra_u32(ram + 0x87710 + d + 4 * i), a; int k;
      if (tra_ptr(p)) { a = p & 0x1FFFFF; for (k = 0; k < 40 && a + k < 0x200000 && ram[a + k] >= 32 && ram[a + k] < 127; k++) out[o++] = ram[a + k]; }
      out[o++] = '\n';
   }
   out[o] = 0;
   return (*e)->NewStringUTF(e, out);
}

/* A plain-text report of what the emulator and the analog installer see (Settings > Diagnostics). */
JNIEXPORT jstring JNICALL J(diagnostics)(JNIEnv *e, jclass c)
{
   static char out[12000];
   size_t o = 0; int i;
   uint8_t *ram = g_loaded ? (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) : NULL;
   uint8_t *card = g_loaded ? (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) : NULL;
#define P(...) do { if (o < sizeof(out)) o += snprintf(out + o, sizeof(out) - o, __VA_ARGS__); } while (0)
   P("loaded %d  game %s  cheats %d\n", g_loaded, g_loaded ? CdromId : "-", g_cheats_set);
   if (ram) {
      uint32_t slot, w; int first = -1, nz = 0;
      memcpy(&slot, ram + XGA_SLOT, 4);
      for (i = 0; i < g_xga_len; i++) { if (ram[XGA_ADDR + i] != g_xga_prog[i] && first < 0) first = i; if (ram[XGA_ADDR + i]) nz++; }
      P("analog: on %d state %d pad %d opts %x len %d | sig ", g_xga_on, g_xga_state, g_xga_pad, g_xga_opts, g_xga_len);
      for (i = 0; i < 8; i++) P("%02x", ram[0x36388 + i]);
      P(" | slot %08x | block nonzero %d, first diff %s", slot, nz, first < 0 ? "none" : "");
      if (first >= 0) {
         P("%08x\n  ram ", 0x80000000u | (XGA_ADDR + first));
         for (i = 0; i < 16 && first + i < g_xga_len; i++) P("%02x", ram[XGA_ADDR + first + i]);
         P("\n  want");
         for (i = 0; i < 16 && first + i < g_xga_len; i++) P("%02x", g_xga_prog[first + i]);
      }
      P("\n  block 80017400: ");
      for (i = 0; i < 32; i++) P("%02x", ram[XGA_ADDR + i]);
      P("\n  state 80017F00: ");
      for (i = 0; i < 32; i++) P("%02x", ram[0x17F00 + i]);
      memcpy(&w, ram + 0x6FAF0, 4);
      P("\n  program %08x room %04x lock %02x%02x pad rx %02x%02x%02x%02x%02x%02x%02x%02x\n", w, ram[0x6F94E] | ram[0x6F94F] << 8,
        ram[0xB2177], ram[0xB2176], ram[0x625FC], ram[0x625FD], ram[0x625FE], ram[0x625FF], ram[0x62600], ram[0x62601], ram[0x62602], ram[0x62603]);
   }
   if (card) {
      int used = 0, x = 0;
      for (i = 0; i < 127; i++) x ^= card[i];
      P("card: status %d size %u hdr %c%c chk %02x/%02x formatted %d |", g_card_status, (unsigned)retro_get_memory_size(RETRO_MEMORY_SAVE_RAM),
        card[0] >= 32 ? card[0] : '?', card[1] >= 32 ? card[1] : '?', card[127], x, card_formatted(card));
      for (i = 1; i < 16; i++) {
         const uint8_t *d = card + i * 128;
         if ((d[0] & 0xF0) == 0x50) { used++; P(" %d:%02x %.20s", i, d[0], (const char *)d + 10); }
         else if (d[0] != 0xA0) P(" %d:%02x", i, d[0]);
      }
      P(" | used %d\n", used);
   }
   P("core log:\n");
   pthread_mutex_lock(&g_log_lock);
   for (i = g_loghead > LOGRING ? g_loghead - LOGRING : 0; i < g_loghead; i++) P("  %s\n", g_logring[i % LOGRING]);
   pthread_mutex_unlock(&g_log_lock);
#undef P
   for (i = 0; out[i]; i++) if (out[i] != '\n' && ((unsigned char)out[i] < 0x20 || (unsigned char)out[i] > 0x7e)) out[i] = '?';   /* JNI wants plain text */
   return (*e)->NewStringUTF(e, out);
}


JNIEXPORT void JNICALL J(runFrame)(JNIEnv *e, jclass c) { if (g_loaded) { McdFlag[0] &= ~8; McdFlag[1] &= ~8; trd_tick(); tra_tick(); retro_run(); xga_tick(); } }

JNIEXPORT jdouble JNICALL J(fps)(JNIEnv *e, jclass c) { return g_av.timing.fps > 1 ? g_av.timing.fps : 59.94; }
JNIEXPORT jdouble JNICALL J(sampleRate)(JNIEnv *e, jclass c) { return g_av.timing.sample_rate > 1 ? g_av.timing.sample_rate : 44100; }
JNIEXPORT jfloat JNICALL J(aspect)(JNIEnv *e, jclass c) { return g_av.geometry.aspect_ratio > 0 ? g_av.geometry.aspect_ratio : 4.0f / 3.0f; }

/* Copies the newest frame into a direct ByteBuffer if it changed since lastSeq.
 * Returns packed (w << 16 | h), or 0 if nothing new. */
JNIEXPORT jint JNICALL J(copyFrame)(JNIEnv *e, jclass c, jobject buf, jint lastSeq, jintArray seqOut)
{
   uint16_t *dst = (*e)->GetDirectBufferAddress(e, buf);
   jlong cap = (*e)->GetDirectBufferCapacity(e, buf);
   jint r = 0, seq;
   if (!dst) return 0;
   pthread_mutex_lock(&g_fb_lock);
   seq = g_fb_seq;
   if (seq != lastSeq && g_fb_w && (jlong)g_fb_w * g_fb_h * 2 <= cap) {
      memcpy(dst, g_fb, (size_t)g_fb_w * g_fb_h * 2);
      r = (g_fb_w << 16) | g_fb_h;
   }
   pthread_mutex_unlock(&g_fb_lock);
   (*e)->SetIntArrayRegion(e, seqOut, 0, 1, &seq);
   return r;
}

JNIEXPORT jint JNICALL J(readAudio)(JNIEnv *e, jclass c, jshortArray out)
{
   jint max = (*e)->GetArrayLength(e, out) / 2, n = 0;
   jshort *o = (*e)->GetShortArrayElements(e, out, NULL);
   pthread_mutex_lock(&g_ar_lock);
   while (n < max && g_ar_r != g_ar_w) {
      o[n * 2] = g_ar[g_ar_r * 2]; o[n * 2 + 1] = g_ar[g_ar_r * 2 + 1];
      g_ar_r = (g_ar_r + 1) % AR_FRAMES; n++;
   }
   pthread_mutex_unlock(&g_ar_lock);
   (*e)->ReleaseShortArrayElements(e, out, o, 0);
   return n * 2;
}

JNIEXPORT jint JNICALL J(audioQueued)(JNIEnv *e, jclass c)
{
   int w = g_ar_w, r = g_ar_r;
   return (w - r + AR_FRAMES) % AR_FRAMES;
}

JNIEXPORT void JNICALL J(setButtons)(JNIEnv *e, jclass c, jint port, jint mask) { if (port >= 0 && port < 2) g_btn[port] = (uint32_t)mask; }
JNIEXPORT void JNICALL J(setAxes)(JNIEnv *e, jclass c, jint port, jint lx, jint ly, jint rx, jint ry)
{
   if (port < 0 || port > 1) return;
   g_axis[port][0] = lx; g_axis[port][1] = ly; g_axis[port][2] = rx; g_axis[port][3] = ry;
}
JNIEXPORT void JNICALL J(setStickDpad)(JNIEnv *e, jclass c, jboolean on, jint thresh) { g_stick_dpad = on; g_stick_dpad_thresh = thresh; }

/* ---- RAM access for the Link companion ---- */
static uint8_t *ram_ptr(uint32_t addr, uint32_t len)
{
   uint8_t *ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   uint32_t a;
   if (!ram || !g_loaded) return NULL;
   /* KUSEG / KSEG0 / KSEG1 mirrors of the 2MB main RAM */
   a = addr & 0x1FFFFFFF;
   if (a >= 0x00800000) return NULL;
   a &= 0x1FFFFF;
   if (a + len > 0x200000) return NULL;
   return ram + a;
}

JNIEXPORT jbyteArray JNICALL J(readRam)(JNIEnv *e, jclass c, jint addr, jint len)
{
   uint8_t *p; jbyteArray out;
   if (len <= 0 || len > 0x200000) return NULL;
   p = ram_ptr((uint32_t)addr, (uint32_t)len);
   if (!p) return NULL;
   out = (*e)->NewByteArray(e, len);
   (*e)->SetByteArrayRegion(e, out, 0, len, (const jbyte *)p);
   return out;
}

JNIEXPORT jboolean JNICALL J(writeRam)(JNIEnv *e, jclass c, jint addr, jbyteArray data)
{
   jint len = (*e)->GetArrayLength(e, data);
   uint8_t *p = ram_ptr((uint32_t)addr, (uint32_t)len);
   if (!p) return JNI_FALSE;
   (*e)->GetByteArrayRegion(e, data, 0, len, (jbyte *)p);
   return JNI_TRUE;
}

JNIEXPORT jstring JNICALL J(gameId)(JNIEnv *e, jclass c) { return (*e)->NewStringUTF(e, CdromId); }
JNIEXPORT jstring JNICALL J(gameLabel)(JNIEnv *e, jclass c) { return (*e)->NewStringUTF(e, CdromLabel); }

JNIEXPORT jstring JNICALL J(pollMessage)(JNIEnv *e, jclass c, jintArray seqInOut)
{
   jint seq; (*e)->GetIntArrayRegion(e, seqInOut, 0, 1, &seq);
   if (seq == g_msg_seq) return NULL;
   seq = g_msg_seq; (*e)->SetIntArrayRegion(e, seqInOut, 0, 1, &seq);
   return (*e)->NewStringUTF(e, g_msg);
}

/* ---- memory card / save states ---- */
JNIEXPORT jboolean JNICALL J(saveMemcard)(JNIEnv *e, jclass c)
{
   void *sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   size_t sz = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
   if (!g_loaded || !sram || !sz || !g_srm_path[0]) return JNI_FALSE;
   return save_file(g_srm_path, sram, sz) ? JNI_TRUE : JNI_FALSE;
}

/* cheap change detector so Java only writes the card when it changed */
JNIEXPORT jlong JNICALL J(memcardHash)(JNIEnv *e, jclass c)
{
   const uint8_t *p = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   size_t sz = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM), i;
   uint64_t h = 1469598103934665603ULL;
   if (!g_loaded || !p) return 0;
   for (i = 0; i < sz; i++) { h ^= p[i]; h *= 1099511628211ULL; }
   return (jlong)h;
}

JNIEXPORT jboolean JNICALL J(saveState)(JNIEnv *e, jclass c, jstring jpath)
{
   char path[600]; size_t sz; void *buf; jboolean ok = JNI_FALSE;
   if (!g_loaded) return JNI_FALSE;
   jstr(e, jpath, path, sizeof(path));
   sz = retro_serialize_size();
   if (!sz || !(buf = malloc(sz))) return JNI_FALSE;
   if (retro_serialize(buf, sz) && save_file(path, buf, sz)) ok = JNI_TRUE;
   free(buf);
   return ok;
}

/* The state as bytes, so a save can be written to disk off the emulator thread (the safety save while playing):
 * serializing takes a few ms, writing 4 MB to storage much longer. NULL if there is no game or no memory. */
JNIEXPORT jbyteArray JNICALL J(stateBytes)(JNIEnv *e, jclass c)
{
   size_t sz; void *buf; jbyteArray out = NULL;
   if (!g_loaded) return NULL;
   sz = retro_serialize_size();
   if (!sz || !(buf = malloc(sz))) return NULL;
   if (retro_serialize(buf, sz) && (out = (*e)->NewByteArray(e, (jsize)sz)) != NULL)
      (*e)->SetByteArrayRegion(e, out, 0, (jsize)sz, (const jbyte *)buf);
   free(buf);
   return out;
}

JNIEXPORT jboolean JNICALL J(loadState)(JNIEnv *e, jclass c, jstring jpath)
{
   char path[600]; size_t sz; void *buf; FILE *f; long fl; jboolean ok = JNI_FALSE;
   if (!g_loaded) return JNI_FALSE;
   jstr(e, jpath, path, sizeof(path));
   f = fopen(path, "rb");
   if (!f) return JNI_FALSE;
   fseek(f, 0, SEEK_END); fl = ftell(f); fseek(f, 0, SEEK_SET);
   sz = retro_serialize_size();
   if (fl > 0 && (size_t)fl <= sz + 4096 && (buf = calloc(1, sz > (size_t)fl ? sz : (size_t)fl))) {
      if (fread(buf, 1, fl, f) == (size_t)fl && retro_unserialize(buf, fl)) { ok = JNI_TRUE; g_xga_pad = 0; }
      free(buf);
   }
   fclose(f);
   return ok;
}

JNIEXPORT void JNICALL J(reset)(JNIEnv *e, jclass c) { if (g_loaded) { retro_reset(); g_xga_pad = 0; } }

JNIEXPORT void JNICALL J(setCheat)(JNIEnv *e, jclass c, jint index, jboolean enabled, jstring code)
{
   char buf[2048];
   if (!g_loaded) return;
   jstr(e, code, buf, sizeof(buf));
   retro_cheat_set((unsigned)index, enabled, buf);
   if (index + 1 > g_cheats_set) g_cheats_set = index + 1;
}
JNIEXPORT void JNICALL J(clearCheats)(JNIEnv *e, jclass c) { if (g_loaded) retro_cheat_reset(); g_cheats_set = 0; }

/* ---- discs ---- */
JNIEXPORT jint JNICALL J(discCount)(JNIEnv *e, jclass c) { return g_disk_have && g_disk.get_num_images ? (jint)g_disk.get_num_images() : 0; }
JNIEXPORT jint JNICALL J(discIndex)(JNIEnv *e, jclass c) { return g_disk_have && g_disk.get_image_index ? (jint)g_disk.get_image_index() : 0; }

JNIEXPORT jboolean JNICALL J(setDisc)(JNIEnv *e, jclass c, jint idx)
{
   bool ok;
   if (!g_disk_have || !g_disk.set_eject_state || !g_disk.set_image_index) return JNI_FALSE;
   g_disk.set_eject_state(true);
   ok = g_disk.set_image_index((unsigned)idx);
   g_disk.set_eject_state(false);
   return ok ? JNI_TRUE : JNI_FALSE;
}

/* Adds a disc image (e.g. disc 2) and switches to it. */
JNIEXPORT jboolean JNICALL J(addDisc)(JNIEnv *e, jclass c, jstring jpath)
{
   char path[600]; struct retro_game_info gi; unsigned idx; bool ok;
   if (!g_disk_have || !g_disk.add_image_index || !g_disk.replace_image_index) return JNI_FALSE;
   jstr(e, jpath, path, sizeof(path));
   memset(&gi, 0, sizeof(gi)); gi.path = path;
   g_disk.set_eject_state(true);
   idx = g_disk.get_num_images();
   ok = g_disk.add_image_index() && g_disk.replace_image_index(idx, &gi) && g_disk.set_image_index(idx);
   g_disk.set_eject_state(false);
   return ok ? JNI_TRUE : JNI_FALSE;
}

/* Queues another disc image (e.g. disc 2) without inserting it. Returns its index or -1. */
JNIEXPORT jint JNICALL J(appendDisc)(JNIEnv *e, jclass c, jstring jpath)
{
   char path[600]; struct retro_game_info gi; unsigned idx;
   if (!g_disk_have || !g_disk.add_image_index || !g_disk.replace_image_index || !g_disk.get_num_images) return -1;
   jstr(e, jpath, path, sizeof(path));
   memset(&gi, 0, sizeof(gi)); gi.path = path;
   idx = g_disk.get_num_images();
   if (!g_disk.add_image_index() || !g_disk.replace_image_index(idx, &gi)) return -1;
   return (jint)idx;
}

/* Puts port 1 in DualShock analog mode (on) or digital mode. Xenogears never switches the pad itself. */
JNIEXPORT void JNICALL J(setAnalogMode)(JNIEnv *e, jclass c, jboolean on)
{
   int r;
   if (!g_loaded) return;
   r = padToggleAnalog(0);
   if ((r != 0) != (on != 0)) padToggleAnalog(0);
}

JNIEXPORT jint JNICALL J(cardStatus)(JNIEnv *e, jclass c) { return g_card_status; }

/* the live memory card (card 1) while a game is loaded */
JNIEXPORT jbyteArray JNICALL J(readMemcard)(JNIEnv *e, jclass c)
{
   void *sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   jbyteArray out;
   if (!g_loaded || !sram || retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) != MCD_BYTES) return NULL;
   out = (*e)->NewByteArray(e, MCD_BYTES);
   (*e)->SetByteArrayRegion(e, out, 0, MCD_BYTES, (const jbyte *)sram);
   return out;
}

JNIEXPORT jboolean JNICALL J(writeMemcard)(JNIEnv *e, jclass c, jbyteArray data)
{
   void *sram = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   if (!g_loaded || !sram || (*e)->GetArrayLength(e, data) != MCD_BYTES) return JNI_FALSE;
   (*e)->GetByteArrayRegion(e, data, 0, MCD_BYTES, (jbyte *)sram);
   return save_file(g_srm_path, sram, MCD_BYTES) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL J(unload)(JNIEnv *e, jclass c)
{
   if (!g_loaded) return;
   J(saveMemcard)(e, c);
   retro_unload_game();
   g_loaded = 0;
}

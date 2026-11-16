/* linkscr.c - Tomb Raider Link on the 3DS's bottom screen (320 x 240): the mini map and the story steps of
 * app/www/index.html, ported to C. The rules (what counts as taken, when a step is done, the route over the walkable boxes) follow
 * the page line by line; the function names are the page's. The map pictures, names and letters come from tomb-raider-link.pak,
 * made from the player's disc by tools/mk_pak.js; the steps from app/www/hints.js (tools/mk_hints.js, at build time).
 * Without the pack the screen still reads the game and shows the steps, in the system font, with no map pictures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "linkscr.h"
#ifdef __3DS__
#include <3ds.h>
#endif

typedef struct { const char *text; int room, x, y, z, nit, it[8], nw; const int32_t (*w)[4]; } TrStep;
typedef struct { const TrStep *s; int n; } TrLevelSteps;
#include "hintdata.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define RGB(r, g, b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
static const uint16_t C_BG = RGB(0x0c, 0x0a, 0x08), C_GOLD = RGB(0xe2, 0xbf, 0x6a), C_GOLD2 = RGB(0xa8, 0x80, 0x3a), C_INK = RGB(0xef, 0xe6, 0xd2),
    C_DIM = RGB(0xa9, 0x9c, 0x84), C_GREEN = RGB(0x7f, 0xe0, 0x8a), C_RED = RGB(0xe0, 0x70, 0x5a), C_SHADOW = RGB(0x3a, 0x14, 0x06),
    C_PANEL = RGB(14, 11, 8), C_BTN = RGB(30, 24, 16), C_LINE = RGB(0x5a, 0x4b, 0x2c);

/* ================================================================== drawing kit */
static uint16_t *fb;
static int clipX0, clipY0, clipX1 = LINK_W, clipY1 = LINK_H;
static void clip(int x0, int y0, int x1, int y1) { clipX0 = x0 < 0 ? 0 : x0; clipY0 = y0 < 0 ? 0 : y0; clipX1 = x1 > LINK_W ? LINK_W : x1; clipY1 = y1 > LINK_H ? LINK_H : y1; }
static void noclip(void) { clip(0, 0, LINK_W, LINK_H); }
static inline uint16_t mix(uint16_t a, uint16_t b, int t) {   /* a over b, t of 256 */
    int r = ((a >> 11) * t + (b >> 11) * (256 - t)) >> 8, g = (((a >> 5) & 63) * t + ((b >> 5) & 63) * (256 - t)) >> 8, bl = ((a & 31) * t + (b & 31) * (256 - t)) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline void put(int x, int y, uint16_t c) { if (x >= clipX0 && x < clipX1 && y >= clipY0 && y < clipY1) fb[y * LINK_W + x] = c; }
static inline void putA(int x, int y, uint16_t c, int a) { if (x >= clipX0 && x < clipX1 && y >= clipY0 && y < clipY1) { uint16_t *p = fb + y * LINK_W + x; *p = mix(c, *p, a); } }
static void rect(int x, int y, int w, int h, uint16_t c) {
    int x0 = x < clipX0 ? clipX0 : x, y0 = y < clipY0 ? clipY0 : y, x1 = x + w > clipX1 ? clipX1 : x + w, y1 = y + h > clipY1 ? clipY1 : y + h;
    for (int j = y0; j < y1; j++) for (int i = x0; i < x1; i++) fb[j * LINK_W + i] = c;
}
static void rectA(int x, int y, int w, int h, uint16_t c, int a) {
    int x0 = x < clipX0 ? clipX0 : x, y0 = y < clipY0 ? clipY0 : y, x1 = x + w > clipX1 ? clipX1 : x + w, y1 = y + h > clipY1 ? clipY1 : y + h;
    for (int j = y0; j < y1; j++) for (int i = x0; i < x1; i++) { uint16_t *p = fb + j * LINK_W + i; *p = mix(c, *p, a); }
}
static void frame(int x, int y, int w, int h, uint16_t c) { rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c); rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c); }
static void frameA(int x, int y, int w, int h, uint16_t c, int a) { rectA(x, y, w, 1, c, a); rectA(x, y + h - 1, w, 1, c, a); rectA(x, y + 1, 1, h - 2, c, a); rectA(x + w - 1, y + 1, 1, h - 2, c, a); }
/* a line w pixels wide; dash > 0: dash on, gap off, counted from *phase */
static void lineW(float x0, float y0, float x1, float y1, int w, uint16_t c, int a, int dash, int gap, float *phase) {
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy); int n = (int)(len * 1.4f) + 1; float ph = phase ? *phase : 0;
    if (len > 2000) return;
    for (int s = 0; s <= n; s++) {
        float t = (float)s / n, d = ph + len * t;
        if (dash && fmodf(d, (float)(dash + gap)) >= dash) continue;
        int x = (int)floorf(x0 + dx * t), y = (int)floorf(y0 + dy * t);
        for (int oy = -(w - 1) / 2; oy <= w / 2; oy++) for (int ox = -(w - 1) / 2; ox <= w / 2; ox++) a >= 256 ? put(x + ox, y + oy, c) : putA(x + ox, y + oy, c, a);
    }
    if (phase) *phase = ph + len;
}
static void line1(int x0, int y0, int x1, int y1, uint16_t c) { lineW((float)x0, (float)y0, (float)x1, (float)y1, 1, c, 256, 0, 0, NULL); }
static void disc(float cx, float cy, float r, uint16_t c, int a) {
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++) for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy; if (dx * dx + dy * dy <= r * r) a >= 256 ? put(x, y, c) : putA(x, y, c, a);
    }
}
static void ring(float cx, float cy, float r, float w, uint16_t c, int a) {
    float ro = r + w / 2, ri = r - w / 2; if (ri < 0) ri = 0;
    for (int y = (int)floorf(cy - ro); y <= (int)ceilf(cy + ro); y++) for (int x = (int)floorf(cx - ro); x <= (int)ceilf(cx + ro); x++) {
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = dx * dx + dy * dy; if (d <= ro * ro && d >= ri * ri) a >= 256 ? put(x, y, c) : putA(x, y, c, a);
    }
}
/* a filled polygon (even-odd), points as x, y pairs */
static void poly(const float *p, int n, uint16_t c, int a) {
    float y0 = 1e9f, y1 = -1e9f; for (int i = 0; i < n; i++) { if (p[2 * i + 1] < y0) y0 = p[2 * i + 1]; if (p[2 * i + 1] > y1) y1 = p[2 * i + 1]; }
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1); y++) {
        float yc = y + 0.5f, xs[16]; int m = 0;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float ya = p[2 * j + 1], yb = p[2 * i + 1];
            if ((ya <= yc) != (yb <= yc) && m < 16) xs[m++] = p[2 * j] + (yc - ya) / (yb - ya) * (p[2 * i] - p[2 * j]);
        }
        for (int i = 1; i < m; i++) for (int k = i; k > 0 && xs[k - 1] > xs[k]; k--) { float t = xs[k]; xs[k] = xs[k - 1]; xs[k - 1] = t; }
        for (int i = 0; i + 1 < m; i += 2) for (int x = (int)ceilf(xs[i] - 0.5f); x < (int)ceilf(xs[i + 1] - 0.5f); x++) a >= 256 ? put(x, y, c) : putA(x, y, c, a);
    }
}
static void polyLine(const float *p, int n, uint16_t c) { for (int i = 0, j = n - 1; i < n; j = i++) lineW(p[2 * j], p[2 * j + 1], p[2 * i], p[2 * i + 1], 1, c, 256, 0, 0, NULL); }
static void star(float x, float y, float r, uint16_t c, int a) {
    float p[20]; for (int i = 0; i < 10; i++) { float ang = -(float)M_PI / 2 + i * (float)M_PI / 5, rr = i & 1 ? r * 0.45f : r; p[2 * i] = x + cosf(ang) * rr; p[2 * i + 1] = y + sinf(ang) * rr; }
    poly(p, 10, c, a);
}
static void tick(int x, int y, uint16_t c) { lineW((float)x, (float)y + 5, (float)x + 3, (float)y + 8, 2, c, 256, 0, 0, NULL); lineW((float)x + 3, (float)y + 8, (float)x + 9, (float)y + 1, 2, c, 256, 0, 0, NULL); }

/* ---- text: the game's letters from the pack (rows of lit pixels, 12 pixels for a capital), with the game's dark edge; without
   the pack the system's 8 x 8 font */
typedef struct { int8_t top; uint8_t w, h, adv; uint16_t row[16]; } Glyph;
static Glyph GL[128]; static int haveFont;
#ifndef __3DS__
const uint8_t *host_font;                 /* the PC test's copy of the system font (linkhost.c), or NULL */
#endif
static const uint8_t *sysFont(void) {
#ifdef __3DS__
    static const uint8_t *f; if (!f) f = (const uint8_t *)consoleGetDefault()->font.gfx; return f;
#else
    return host_font;
#endif
}
#define LINEH 14
static int glyphAdv(int ch) { if (!haveFont) return 8; if (ch < 0 || ch > 127) ch = '?'; return GL[ch].adv ? GL[ch].adv : 6; }
static int textW(const char *s) { int w = 0; for (; *s; s++) w += glyphAdv((unsigned char)*s); return w; }
/* colour by row of the letter: kind 0 flat, 1 the page's gold (light at the top, dark at the foot) */
static uint16_t goldRow(int r) {
    static uint16_t T[16]; static int made;
    if (!made) {
        made = 1;
        for (int i = 0; i < 16; i++) {
            float t = i / 11.0f; int R, G, B;
            if (t < 0.45f) { float u = t / 0.45f; R = 0xff + (0xe2 - 0xff) * u; G = 0xf2 + (0xbf - 0xf2) * u; B = 0xc4 + (0x6a - 0xc4) * u; }
            else { float u = (t - 0.45f) / 0.55f; if (u > 1) u = 1; R = 0xe2 + (0xa8 - 0xe2) * u; G = 0xbf + (0x80 - 0xbf) * u; B = 0x6a + (0x3a - 0x6a) * u; }
            T[i] = RGB(R, G, B);
        }
    }
    return T[r < 0 ? 0 : r > 15 ? 15 : r];
}
static void glyphAt(int ch, int x, int base, uint16_t c, int gold, int shadow) {
    if (!haveFont) {
        const uint8_t *g = sysFont(); if (!g) return; g += (ch & 255) * 8;
        for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) if (g[j] & (0x80 >> i)) {
            if (shadow) put(x + i + 1, base - 8 + j + 1, C_SHADOW);
            put(x + i, base - 8 + j, gold ? goldRow(j * 3 / 2) : c);
        }
        return;
    }
    if (ch < 0 || ch > 127) ch = '?';
    const Glyph *g = GL + ch; int y0 = base - g->top;
    for (int j = 0; j < g->h; j++) for (int i = 0; i < g->w; i++) if (g->row[j] & (1 << i)) {
        if (shadow) put(x + i + 1, y0 + j + 1, C_SHADOW);
        put(x + i, y0 + j, gold ? goldRow(j + (11 - g->top)) : c);
    }
}
/* y is the top of a LINEH line; the baseline sits 12 pixels down. The edge falls right and down, so letters drawn later (and
   the lower rows of the same letter) always cover it */
static int text(const char *s, int x, int y, uint16_t c) {
    int b = y + 12, xx = x;
    for (const char *p = s; *p; p++) { if (*p > ' ') glyphAt((unsigned char)*p, xx, b, c, 0, 1); xx += glyphAdv((unsigned char)*p); }
    return xx;
}
static int textGold(const char *s, int x, int y) {
    int b = y + 12, xx = x;
    for (const char *p = s; *p; p++) { if (*p > ' ') glyphAt((unsigned char)*p, xx, b, 0, 1, 1); xx += glyphAdv((unsigned char)*p); }
    return xx;
}
static void textC(const char *s, int cx, int y, uint16_t c) { text(s, cx - textW(s) / 2, y, c); }
static void textR(const char *s, int xr, int y, uint16_t c) { text(s, xr - textW(s), y, c); }
/* words wrapped to w pixels; draw: 0 only counts the lines. Returns the number of lines */
static int wrap(const char *s, int x, int y, int w, int maxLines, uint16_t c, int draw) {
    char ln[160]; int lines = 0; const char *p = s;
    while (*p && lines < maxLines) {
        int n = 0, lastSp = -1, wd = 0; const char *q = p;
        while (*q && n < (int)sizeof ln - 4) {
            int a = glyphAdv((unsigned char)*q);
            if (*q == ' ') lastSp = n;
            if (wd + a > w && n) break;
            ln[n++] = *q++; wd += a;
        }
        if (*q && lastSp > 0 && *q != ' ') { n = lastSp; q = p + lastSp + 1; }
        ln[n] = 0;
        if (lines == maxLines - 1 && *q) {   /* the last line that fits: cut with an ellipsis */
            while (n > 0 && textW(ln) + 3 * glyphAdv('.') > w) ln[--n] = 0;
            strcat(ln, "...");
        }
        if (draw) text(ln, x, y + lines * LINEH, c);
        lines++; p = q; while (*p == ' ') p++;
    }
    return lines;
}

/* ================================================================== the pack */
static char DIR[256];
typedef struct { char name[24]; uint32_t off, len; } PakEnt;
static PakEnt *pakDir; static int pakN; static FILE *pakF;
static void *pakRead(const char *name, uint32_t *len) {
    if (!pakF) return NULL;
    for (int i = 0; i < pakN; i++) if (!strcmp(pakDir[i].name, name)) {
        void *b = malloc(pakDir[i].len + 4); if (!b) return NULL;
        if (fseek(pakF, pakDir[i].off, SEEK_SET) || fread(b, 1, pakDir[i].len, pakF) != pakDir[i].len) { free(b); return NULL; }
        if (len) *len = pakDir[i].len; return b;
    }
    return NULL;
}
static void pakOpen(void) {
    char p[320]; snprintf(p, sizeof p, "%s/tomb-raider-link.pak", DIR);
    pakF = fopen(p, "rb"); if (!pakF) return;
    char h[12]; if (fread(h, 1, 12, pakF) != 12 || memcmp(h, "TRPK", 4)) { fclose(pakF); pakF = NULL; return; }
    int32_t ver, n; memcpy(&ver, h + 4, 4); memcpy(&n, h + 8, 4);
    if (ver != 1 || n <= 0 || n > 256) { fclose(pakF); pakF = NULL; return; }
    pakDir = calloc(n, sizeof *pakDir); pakN = n;
    for (int i = 0; i < n; i++) { char e[32]; if (fread(e, 1, 32, pakF) != 32) { pakN = i; break; } memcpy(pakDir[i].name, e, 24); pakDir[i].name[23] = 0; memcpy(&pakDir[i].off, e + 24, 4); memcpy(&pakDir[i].len, e + 28, 4); }
}
/* names: the 22 level names, then per level the key item names by type */
static char LVNAME[22][40];
typedef struct { int lv, type; char name[40]; } ItemName;
static ItemName *INAME; static int nIName;
static const char *levelName(int n) { return n >= 0 && n < 22 && LVNAME[n][0] ? LVNAME[n] : NULL; }
static const char *itemName(int lv, int type) { for (int i = 0; i < nIName; i++) if (INAME[i].lv == lv && INAME[i].type == type) return INAME[i].name; return ""; }
typedef struct { int lv, type, w, h; const uint16_t *px; } Icon;
static Icon *ICONS; static int nIcons; static void *iconBlob;
static const int PERLEVEL[] = { 110, 111, 112, 113, 126, 129, 130, 131, 132, 143, 144, 145 };
static const int KEYT[] = { 84, 85, 86, 87, 110, 111, 112, 113, 126, 129, 130, 131, 132, 143, 144, 145 };
static const int SUPT[] = { 88, 89, 90, 91, 93, 94 };
static int inList(const int *a, int n, int v) { for (int i = 0; i < n; i++) if (a[i] == v) return 1; return 0; }
#define IN(a, v) inList(a, (int)(sizeof a / sizeof a[0]), v)
static const Icon *iconFor(int lv, int type) {
    int own = IN(PERLEVEL, type);
    for (int i = 0; i < nIcons; i++) if (ICONS[i].type == type && ICONS[i].lv == (own ? lv : -1)) return ICONS + i;
    return NULL;
}
static void pakLoadCommon(void) {
    uint32_t n; int32_t *b;
    if ((b = pakRead("names", &n))) {
        const char *c = (const char *)b; for (int i = 0; i < 22; i++) memcpy(LVNAME[i], c + 40 * i, 40), LVNAME[i][39] = 0;
        const char *p = c + 22 * 40; int cap = 0;
        for (int lv = 0; lv < 16 && p < c + n; lv++) {
            int32_t k; memcpy(&k, p, 4); p += 4;
            for (int j = 0; j < k; j++, p += 44) {
                if (nIName == cap) { cap = cap ? cap * 2 : 64; INAME = realloc(INAME, cap * sizeof *INAME); }
                ItemName *e = INAME + nIName++; e->lv = lv; memcpy(&e->type, p, 4); memcpy(e->name, p + 4, 40); e->name[39] = 0;
            }
        }
        free(b);
    }
    if ((b = pakRead("icons", &n))) {
        iconBlob = b; int k = b[0]; const int32_t *p = b + 1; ICONS = calloc(k > 0 ? k : 1, sizeof *ICONS);
        for (int i = 0; i < k; i++) { Icon *e = ICONS + nIcons++; e->lv = p[0]; e->type = p[1]; e->w = p[2]; e->h = p[3]; e->px = (const uint16_t *)(p + 4); p = (const int32_t *)((const uint8_t *)(p + 4) + ((e->w * e->h * 2 + 3) & ~3)); }
    }
    uint8_t *f = pakRead("font", &n);
    if (f && n >= 128 * 36) { for (int c = 0; c < 128; c++) { const uint8_t *g = f + 36 * c; GL[c].top = (int8_t)g[0]; GL[c].w = g[1]; GL[c].h = g[2]; GL[c].adv = g[3]; memcpy(GL[c].row, g + 4, 32); } haveFont = GL['A'].h > 0; }
    free(f);
}

/* ================================================================== the level (lv<n>, px<n>) */
typedef struct { const int32_t *v; int nc; const int32_t *cut; int np; const int32_t *p; const int32_t *sb; const int32_t *pic; } Room;   /* v: x z yt yb xs zs alt flags */
#define RX(r) (r)->v[0]
#define RZ(r) (r)->v[1]
#define RYT(r) (r)->v[2]
#define RYB(r) (r)->v[3]
#define RXS(r) (r)->v[4]
#define RZS(r) (r)->v[5]
#define RALT(r) (r)->v[6]
static struct {
    int n, demo, loaded;
    int32_t *lvb; uint16_t *pxb; uint32_t pxLen;
    int nr; Room *R; int *altOf; int *boxRoom;
    int nb; const int32_t *box; const int32_t **ovl; int *novl;
    int ni; const int32_t *item;           /* type room x y z sprite flags */
    int ns; const int32_t *sec; int ne; const int32_t *exit; const int32_t *bg;
    int nw; int *watch; uint8_t *taken; int16_t *sw; int8_t *swSet;
    int snapFor; int16_t snap[8];
    /* secrets grouped as the page's secretList */
    int nsg; struct { int i, room; float x, y, z; } sg[32];
} LV = { .n = -1 };
static const int REC = 72;

/* story progress and settings, kept in tomb-raider-link.cfg */
/* view: 0 the map, 1 the compass (index.html's CFG.view); hinttext: the step's words (0: "Step 3 of 11") */
static struct { int zoom, rot, turnby, reveal, step, pickups, secrets, fold, view, hinttext; } CFG = { 16, 0, 0, 1, 1, 1, 0, 0, 0, 1 };
static uint32_t REACHED[16], MOVED[16]; static uint8_t SEEN[16][32]; static int LASTLV = -1, cfgDirty;
static void cfgSave(void) { cfgDirty = 1; }
static void cfgWrite(void) {
    char p[320]; snprintf(p, sizeof p, "%s/tomb-raider-link.cfg", DIR); FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "zoom=%d\nrot=%d\nturnby=%d\nreveal=%d\nstep=%d\npickups=%d\nsecrets=%d\nfold=%d\nview=%d\nhinttext=%d\nlastlv=%d\n", CFG.zoom, CFG.rot, CFG.turnby, CFG.reveal, CFG.step, CFG.pickups, CFG.secrets, CFG.fold, CFG.view, CFG.hinttext, LASTLV);
    for (int n = 0; n < 16; n++) {
        fprintf(f, "lv%d=%lx,%lx,", n, (unsigned long)REACHED[n], (unsigned long)MOVED[n]);
        for (int i = 0; i < 32; i++) fprintf(f, "%02x", SEEN[n][i]);
        fputc('\n', f);
    }
    fclose(f); cfgDirty = 0;
}
static void cfgLoad(void) {
    char p[320], l[200]; snprintf(p, sizeof p, "%s/tomb-raider-link.cfg", DIR); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(l, sizeof l, f)) {
        int n; unsigned long a, b; char hex[80];
        if (sscanf(l, "zoom=%d", &CFG.zoom) == 1 || sscanf(l, "rot=%d", &CFG.rot) == 1 || sscanf(l, "turnby=%d", &CFG.turnby) == 1 || sscanf(l, "reveal=%d", &CFG.reveal) == 1 ||
            sscanf(l, "step=%d", &CFG.step) == 1 || sscanf(l, "pickups=%d", &CFG.pickups) == 1 || sscanf(l, "secrets=%d", &CFG.secrets) == 1 || sscanf(l, "fold=%d", &CFG.fold) == 1 ||
            sscanf(l, "view=%d", &CFG.view) == 1 || sscanf(l, "hinttext=%d", &CFG.hinttext) == 1 || sscanf(l, "lastlv=%d", &LASTLV) == 1) continue;
        if (sscanf(l, "lv%d=%lx,%lx,%64s", &n, &a, &b, hex) == 4 && n >= 0 && n < 16) {
            REACHED[n] = (uint32_t)a; MOVED[n] = (uint32_t)b;
            for (int i = 0; i < 32 && hex[2 * i] && hex[2 * i + 1]; i++) { unsigned v; sscanf(hex + 2 * i, "%2x", &v); SEEN[n][i] = (uint8_t)v; }
        }
    }
    fclose(f);
    if (CFG.zoom < 6 || CFG.zoom > 48) CFG.zoom = 16;
}
static int seen(int r) { return r >= 0 && r < 256 && (SEEN[LV.n][r >> 3] >> (r & 7)) & 1; }

static void levelFree(void) {
    free(LV.lvb); free(LV.pxb); free(LV.R); free(LV.altOf); free(LV.boxRoom); free(LV.ovl); free(LV.novl); free(LV.watch); free(LV.taken); free(LV.sw); free(LV.swSet);
    memset(&LV, 0, sizeof LV); LV.n = -1;
}
static void secretsGroup(void);
static int canon(int r) { return r >= 0 && r < LV.nr && LV.altOf[r] >= 0 ? LV.altOf[r] : r; }
static void levelLoad(int n, int demo) {
    levelFree(); LV.n = n; LV.demo = demo; LV.snapFor = -1;
    if (n < 0 || n > 15) return;
    char nm[8]; uint32_t len = 0;
    snprintf(nm, sizeof nm, "lv%d", n); LV.lvb = pakRead(nm, &len);
    snprintf(nm, sizeof nm, "px%d", n); LV.pxb = pakRead(nm, &LV.pxLen);
    if (LV.lvb) {
        const int32_t *p = LV.lvb;
        LV.nr = *p++; LV.R = calloc(LV.nr, sizeof *LV.R);
        for (int i = 0; i < LV.nr; i++) {
            Room *r = LV.R + i; r->v = p; p += 8;
            r->nc = *p++; r->cut = p; p += r->nc;
            r->np = *p++; r->p = p; p += 8 * r->np;
            r->sb = p; p += RXS(r) * RZS(r);
            r->pic = p; p += 3 * (r->nc + 1);
        }
        LV.nb = *p++; LV.box = p; p += 5 * LV.nb;
        LV.ovl = calloc(LV.nb + 1, sizeof *LV.ovl); LV.novl = calloc(LV.nb + 1, sizeof *LV.novl);
        for (int i = 0; i < LV.nb; i++) { LV.novl[i] = *p++; LV.ovl[i] = p; p += LV.novl[i]; }
        LV.ni = *p++; LV.item = p; p += 7 * LV.ni;
        LV.ns = *p++; LV.sec = p; p += 5 * LV.ns;
        LV.ne = *p++; LV.exit = p; p += 4 * LV.ne;
        LV.bg = p;
        LV.loaded = 1;
    }
    LV.altOf = malloc((LV.nr + 1) * sizeof(int)); for (int i = 0; i < LV.nr; i++) LV.altOf[i] = -1;
    for (int i = 0; i < LV.nr; i++) { int a = RALT(LV.R + i); if (a >= 0 && a < LV.nr) LV.altOf[a] = i; }
    LV.boxRoom = malloc((LV.nb + 1) * sizeof(int)); for (int i = 0; i < LV.nb; i++) LV.boxRoom[i] = -1;
    for (int ri = 0; ri < LV.nr; ri++) { Room *r = LV.R + ri; for (int k = 0; k < RXS(r) * RZS(r); k++) { int b = r->sb[k]; if (b >= 0 && b < LV.nb && LV.boxRoom[b] < 0) LV.boxRoom[b] = ri; } }
    /* the items to watch: key items and supplies with a sprite, and every item a step names */
    LV.watch = malloc((LV.ni + 64) * sizeof(int)); LV.taken = calloc(LV.ni + 1, 1); LV.sw = calloc(LV.ni + 1, 2); LV.swSet = calloc(LV.ni + 1, 1);
    for (int i = 0; i < LV.ni; i++) { const int32_t *it = LV.item + 7 * i; if (it[5] && (IN(KEYT, it[0]) || IN(SUPT, it[0]))) LV.watch[LV.nw++] = i; }
    const TrLevelSteps *H = HINTS + n;
    for (int s = 0; s < H->n; s++) for (int k = 0; k < H->s[s].nit; k++) {
        int it = H->s[s].it[k], have = 0; if (it < 0 || it >= LV.ni) continue;
        for (int j = 0; j < LV.nw; j++) if (LV.watch[j] == it) have = 1;
        if (!have) LV.watch[LV.nw++] = it;
    }
    secretsGroup();
    if (!demo) {
        if (LASTLV != n) { REACHED[n] = 0; MOVED[n] = 0; memset(SEEN[n], 0, sizeof SEEN[n]); LASTLV = n; cfgSave(); }
    }
}

/* ================================================================== reading the game */
static const uint8_t *RAM;
static inline unsigned r8(uint32_t a) { return RAM[a & 0x1FFFFF]; }
static inline unsigned r16(uint32_t a) { a &= 0x1FFFFF; if (a > 0x1FFFFE) return 0; return RAM[a] | RAM[a + 1] << 8; }
static inline int rs16(uint32_t a) { return (int16_t)r16(a); }
static inline uint32_t r32(uint32_t a) { a &= 0x1FFFFF; if (a > 0x1FFFFC) return 0; uint32_t v; memcpy(&v, RAM + a, 4); return v; }
static inline int32_t rs32(uint32_t a) { return (int32_t)r32(a); }
static int ptrOk(uint32_t p) { return p >= 0x80000000u && p < 0x80200000u; }
/* Rev 6's addresses; v1.0 has those below 0x89000 0xE8 earlier and the rest 0x100 earlier (index.html's A0 and REVS) */
enum { A_ENDING, A_LEVEL, A_ITEMS, A_LARA, A_PLAYING, A_FLIP, A_CAM, A_PROG, A_SECRETS, A_RING, A_RINGQ, A_RINGP, A_N };
static const uint32_t A0[A_N] = { 0x87670, 0x87668, 0x89BFC, 0x89FAC, 0x885BA, 0x89D54, 0x89C34, 0x928F8, 0x92900, 0x887C0, 0x887C8, 0x887F8 };
static const struct { const char *name; uint32_t lt; int d, b; } REVS[2] = { { "v1.1 / Rev 5 / Rev 6", 0x87710, 0, 0 }, { "v1.0", 0x87628, -0xE8, -0x100 } };
static uint32_t A[A_N]; static int REV = -1; static uint32_t revSince;
static int revFind(void) { for (int i = 0; i < 2; i++) { uint32_t p = r32(REVS[i].lt), q = r32(REVS[i].lt + 4); if (ptrOk(p) && ptrOk(q) && p - q == 8) return i; } return -1; }
static void revSet(int r) { REV = r; if (r < 0) return; for (int k = 0; k < A_N; k++) A[k] = A0[k] + (A0[k] < 0x89000 ? REVS[r].d : REVS[r].b); }
static int invType(int o) { return o >= 114 && o <= 117 ? o - 4 : o >= 133 && o <= 136 ? o - 4 : o == 127 ? 126 : o == 150 ? 143 : -1; }

static struct { int level, ok, demo, room, x, y, z, hp, flip, ending, unknown, secrets, nheld, held[16][2]; float hd, cam; int camOk; } G = { .level = -1 };
static int inRoom(void) {
    if (!LV.loaded || G.room < 0 || G.room >= LV.nr) return 0; Room *r = LV.R + G.room;
    return G.x >= RX(r) - 1024 && G.x <= RX(r) + RXS(r) * 1024 + 1024 && G.z >= RZ(r) - 1024 && G.z <= RZ(r) + RZS(r) * 1024 + 1024;
}
static void stepUpdate(void);
static void update(uint32_t now) {
    int rv = revFind();
    if (rv >= 0 && rv != REV) revSet(rv);
    if (rv >= 0) revSince = 0; else if (!revSince) revSince = now ? now : 1;
    G.unknown = REV < 0 && revSince && now - revSince > 20000;
    if (REV < 0) { G.level = -1; G.ok = 0; return; }
    int lv = rs16(A[A_LEVEL]); uint32_t ip = r32(A[A_ITEMS]), lp = r32(A[A_LARA]);
    G.level = lv; G.ok = 0;
    if (lv >= 0 && lv <= 15 && ptrOk(ip) && ptrOk(lp) && rs16(lp + 0x0C) == 0) {
        int pl = rs16(A[A_PLAYING]), demo = lv != 0 && pl != -1;    /* the play/demo word stays 1 in Lara's Home, never a demo */
        if (LV.n != lv || LV.demo != demo) levelLoad(lv, demo);
        G.demo = demo; G.room = rs16(lp + 0x18); G.x = rs32(lp + 0x30); G.y = rs32(lp + 0x34); G.z = rs32(lp + 0x38);
        G.hd = r16(lp + 0x3E) / 65536.0f * 2 * (float)M_PI; G.hp = rs16(lp + 0x22);
        G.flip = r8(A[A_FLIP]) == 1; G.ok = LV.loaded ? inRoom() : 1;   /* without the pack there are no rooms to check against */ G.ending = !demo && r8(A[A_ENDING]) == 1;
        int cx0 = rs32(A[A_CAM]), cz0 = rs32(A[A_CAM] + 8), cx1 = rs32(A[A_CAM] + 16), cz1 = rs32(A[A_CAM] + 24);
        G.camOk = cx1 != cx0 || cz1 != cz0; G.cam = G.camOk ? atan2f((float)(cx1 - cx0), (float)(cz1 - cz0)) : 0;
        G.secrets = r8(A[A_SECRETS]); G.nheld = 0;
        int nk = r16(A[A_RING]); if (nk > 16) nk = 16;
        for (int k = 0; k < nk; k++) { uint32_t pp = r32(A[A_RINGP] + 4 * k); int ty = ptrOk(pp) ? invType(r16(pp + 4)) : -1, q = r16(A[A_RINGQ] + 2 * k); if (ty >= 0 && q) { G.held[G.nheld][0] = ty; G.held[G.nheld][1] = q; G.nheld++; } }
        if (LV.loaded && G.ok) {
            for (int w = 0; w < LV.nw; w++) {
                int i = LV.watch[w]; uint32_t a = ip + i * REC; int t = r16(a + 0x0C), st = r16(a + 0x44), fl = r16(a + 0x28), ty = LV.item[7 * i], ph = LV.item[7 * i + 6] & 0x100;
                if (ty == 55 || ty == 56) { LV.sw[i] = (int16_t)rs16(a + 0x0E); LV.swSet[i] = 1; }
                LV.taken[i] = ty == 55 || ty == 56 || (ty >= 74 && ty <= 76) ? (fl & 0x3E00) == 0x3E00 : ty >= 137 && ty <= 140 ? ((st >> 1) & 3) >= 2 :
                    ty >= 118 && ty <= 121 ? t != ty : ((st >> 1) & 3) >= 2 && (!ph || (fl & 0x3E00) == 0x3E00);
            }
            if (!demo) {
                stepUpdate();
                int cr = canon(G.room); if (cr >= 0 && cr < 256 && !seen(cr)) { SEEN[LV.n][cr >> 3] |= 1 << (cr & 7); cfgSave(); }
            }
        }
    } else if (LV.n != -1 && lv != LV.n) levelFree();
}

/* ================================================================== story steps (index.html's, rule for rule) */
#define REACH 1800
static struct { int view, cur, allDone; } STEP = { -1, 0, 0 };
static const TrStep *stepAt(int n, int i) { return HINTS[n].s + i; }
static int usedBefore(int n, int i, int k) {
    if (k < 0 || k >= LV.ni) return 0; int t = LV.item[7 * k]; if (t != 55 && t != 56) return 0;
    for (int j = 0; j < i; j++) { const TrStep *s = stepAt(n, j); for (int q = 0; q < s->nit; q++) if (s->it[q] == k) return 1; }
    return 0;
}
static int stepToggle(int n, int i) { const TrStep *s = stepAt(n, i); if (!s->nit) return 0; for (int q = 0; q < s->nit; q++) if (!usedBefore(n, i, s->it[q])) return 0; return 1; }
static int stepDone(int n, int i) {
    const TrStep *s = stepAt(n, i); int tg = stepToggle(n, i), H = HINTS[n].n;
    if (s->nit && !tg) { for (int q = 0; q < s->nit; q++) { int k = s->it[q]; if (!usedBefore(n, i, k) && !(k >= 0 && k < LV.ni && LV.taken[k])) return 0; } return 1; }
    if (tg ? (MOVED[n] >> i) & 1 : (REACHED[n] >> i) & 1) return 1;
    for (int j = i + 1; j < H; j++) { int tj = stepToggle(n, j); if (tj ? (MOVED[n] >> j) & 1 : stepAt(n, j)->nit ? stepDone(n, j) : (REACHED[n] >> j) & 1) return 1; }
    return 0;
}
static int stepFirst(int n) { for (int i = 0; i < HINTS[n].n; i++) if (!stepDone(n, i)) return i; return -1; }
static void stepMoved(int n) {
    int f = stepFirst(n); if (f < 0 || !stepToggle(n, f)) return; const TrStep *s = stepAt(n, f);
    if (LV.snapFor != f) {
        for (int q = 0; q < s->nit; q++) if (!LV.swSet[s->it[q]]) return;
        LV.snapFor = f; for (int q = 0; q < s->nit && q < 8; q++) LV.snap[q] = LV.sw[s->it[q]];
        return;
    }
    for (int q = 0; q < s->nit && q < 8; q++) if (LV.swSet[s->it[q]] && LV.sw[s->it[q]] != LV.snap[q]) { MOVED[n] |= 1u << f; cfgSave(); return; }
}
static float hyp(float a, float b) { return sqrtf(a * a + b * b); }
static int back(int n, int i) {   /* a step that comes back to an earlier step's place */
    const TrStep *s = stepAt(n, i);
    for (int j = 0; j < i; j++) { const TrStep *t = stepAt(n, j); if (hyp((float)(s->x - t->x), (float)(s->z - t->z)) < REACH && abs(s->y - t->y) < 1400) return 1; }
    return 0;
}
static void stepUpdate(void) {
    int n = LV.n, H = HINTS[n].n; if (!H || !G.ok) return;
    stepMoved(n);
    int f0 = stepFirst(n);
    for (int i = 0; i < H && i < 32; i++) {
        const TrStep *s = stepAt(n, i);
        if (!((REACHED[n] >> i) & 1) && hyp((float)(G.x - s->x), (float)(G.z - s->z)) < REACH && abs(G.y - s->y) < 1400 && (i == f0 || !back(n, i))) { REACHED[n] |= 1u << i; cfgSave(); }
    }
    int cur = H - 1, all = 1; for (int i = 0; i < H; i++) if (!stepDone(n, i)) { cur = i; all = 0; break; }
    if (cur != STEP.cur) { STEP.cur = cur; STEP.view = -1; }
    STEP.allDone = all;
}
static int shownStep(void) { int H = LV.n >= 0 ? HINTS[LV.n].n : 0; if (!H || G.demo) return -1; int i = STEP.view < 0 ? STEP.cur : STEP.view; return i > H - 1 ? H - 1 : i; }
static void stepNav(int d) {
    int H = LV.n >= 0 ? HINTS[LV.n].n : 0; if (!H) return;
    int v = STEP.view < 0 ? STEP.cur : STEP.view; v += d; if (v < 0) v = 0; if (v > H - 1) v = H - 1;
    STEP.view = v == STEP.cur ? -1 : v;
}
static const char *DIRS[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
/* "41 blocks N \x7f 2 up" (the pack's middle dot), or here when nearer than near blocks */
static void wayTo(char *o, int cap, int x, int y, int z, float near, const char *here) {
    float dx = (float)(x - G.x), dz = (float)(z - G.z), dy = (float)(G.y - y), dist = hyp(dx, dz) / 1024, a = atan2f(dx, dz);
    if (dist < near && fabsf(dy) < 768) { snprintf(o, cap, "%s", here); return; }   /* not "here" with the spot a floor above or below */
    int n = (int)lroundf(dist), d = (int)lroundf(fmodf(a * 180 / (float)M_PI + 360, 360) / 45) % 8;
    if (n < 1) { float h = lroundf(fabsf(dy) / 512) / 2.0f; if (h == (int)h) snprintf(o, cap, "%d blocks %s from here", (int)h, dy > 0 ? "up" : "down"); else snprintf(o, cap, "%.1f blocks %s from here", h, dy > 0 ? "up" : "down"); return; }
    int k = snprintf(o, cap, "%d block%s %s", n, n == 1 ? "" : "s", DIRS[d]);
    if (fabsf(dy) >= 768 && k < cap) { float h = lroundf(fabsf(dy) / 512) / 2.0f; if (h == (int)h) snprintf(o + k, cap - k, " %c %d %s", haveFont ? 0x7f : '-', (int)h, dy > 0 ? "up" : "down"); else snprintf(o + k, cap - k, " %c %.1f %s", haveFont ? 0x7f : '-', h, dy > 0 ? "up" : "down"); }
}

/* ================================================================== the way (route, boxPath, routeDrawn, boxNear)
   As index.html since Link 0.13.5 (the map viewer's rule): over the walkable boxes with climbs up to CLIMB, then any climb (high: "high
   climbs"), then a dotted straight line (straight: "no walkable way"). A place not on a box (Lara on a ledge or in water, a spot in a wall)
   uses its room's nearest box, height counted; until 0.2-alpha it used the room openings, whose line cut through walls. */
#define CLIMB 1792
#define MAXPTS 192
typedef struct { int n, rooms, high, straight; float p[MAXPTS][4]; uint8_t set[64]; } Route;   /* p: x y z mark (1 up, -1 down) */
static Route RT; static int rtOk; static char rtKey[64];
static void rtAdd(float x, float y, float z, float m) { if (RT.n < MAXPTS) { RT.p[RT.n][0] = x; RT.p[RT.n][1] = y; RT.p[RT.n][2] = z; RT.p[RT.n][3] = m; RT.n++; } }
static void setRoom(int r) { if (r >= 0 && r < 512) RT.set[r >> 3] |= 1 << (r & 7); }
static int inSet(int r) { return r >= 0 && r < 512 && (RT.set[r >> 3] >> (r & 7)) & 1; }
static int countSet(void) { int c = 0; for (int i = 0; i < 64; i++) for (int b = RT.set[i]; b; b >>= 1) c += b & 1; return c; }
static int boxAt(int room, int x, int z) {
    if (room < 0 || room >= LV.nr) return -1; Room *r = LV.R + room;
    if (G.flip && RALT(r) >= 0 && RALT(r) < LV.nr) r = LV.R + RALT(r);
    int sx = (int)floorf((x - RX(r)) / 1024.0f), sz = (int)floorf((z - RZ(r)) / 1024.0f);
    if (sx < 0 || sz < 0 || sx >= RXS(r) || sz >= RZS(r)) return -1;
    return r->sb[sx * RZS(r) + sz];
}
/* the box under the place, or else its room's box nearest it (height counts too, so a floor far above or below isn't taken) */
static int boxNear(int room, int x, int y, int z) {
    int b = boxAt(room, x, z); if (b >= 0) return b;
    if (room < 0 || room >= LV.nr) return -1; Room *r = LV.R + room;
    if (G.flip && RALT(r) >= 0 && RALT(r) < LV.nr) r = LV.R + RALT(r);
    int best = -1; float bd = 1e30f;
    for (int sx = 0; sx < RXS(r); sx++) for (int sz = 0; sz < RZS(r); sz++) {
        int k = r->sb[sx * RZS(r) + sz]; if (k < 0 || k >= LV.nb) continue;
        float dx = RX(r) + sx * 1024 + 512.0f - x, dz = RZ(r) + sz * 1024 + 512.0f - z, dy = (float)(LV.box[5 * k + 4] - y), d = sqrtf(dx * dx + dz * dz + dy * dy);
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}
static void routeStraight(const TrStep *st) {
    RT.n = 0; memset(RT.set, 0, sizeof RT.set); RT.high = 0; RT.straight = 1;
    rtAdd((float)G.x, (float)G.y, (float)G.z, 0); rtAdd((float)st->x, (float)st->y, (float)st->z, 0); setRoom(canon(G.room)); setRoom(canon(st->room)); RT.rooms = 0;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static int boxPath(int from, int to, int climb, const TrStep *st) {
    int n = LV.nb; static float *dist, (*at)[3]; static int *prev; static uint8_t *done; static int *open; static int cap;
    if (cap < n) { cap = n; dist = realloc(dist, n * sizeof *dist); at = realloc(at, n * sizeof *at); prev = realloc(prev, n * sizeof *prev); done = realloc(done, n); open = realloc(open, n * 8 * sizeof *open); }
    for (int i = 0; i < n; i++) { dist[i] = 1e30f; prev[i] = -1; done[i] = 0; }
    const int32_t *B = LV.box;
    dist[from] = 0; at[from][0] = (float)G.x; at[from][1] = (float)B[5 * from + 4]; at[from][2] = (float)G.z;
    int no = 0; open[no++] = from;
    while (no) {
        int bi = 0; for (int i = 1; i < no; i++) if (dist[open[i]] < dist[open[bi]]) bi = i;
        int c = open[bi]; open[bi] = open[--no]; if (done[c]) continue; done[c] = 1; if (c == to) break;
        const int32_t *A_ = B + 5 * c; float *p = at[c];
        for (int q = 0; q < LV.novl[c]; q++) {
            int k = LV.ovl[c][q]; if (k < 0 || k >= n || done[k]) continue; const int32_t *K = B + 5 * k;
            if (A_[4] - K[4] > climb) continue;   /* y points down: K higher than A by A-K */
            float x = clampf(p[0], (float)(A_[0] > K[0] ? A_[0] : K[0]) - 1, (float)(A_[2] < K[2] ? A_[2] : K[2]) + 1);
            float z = clampf(p[2], (float)(A_[1] > K[1] ? A_[1] : K[1]) - 1, (float)(A_[3] < K[3] ? A_[3] : K[3]) + 1);
            float nd = dist[c] + hyp(x - p[0], z - p[2]) + fabsf((float)(K[4] - A_[4])) * 0.5f + 64;
            if (nd < dist[k]) { dist[k] = nd; prev[k] = c; at[k][0] = x; at[k][1] = (float)K[4]; at[k][2] = z; if (no < n * 8) open[no++] = k; }
        }
    }
    if (!done[to]) return 0;
    static int *chain; static int ccap; if (ccap < n) { ccap = n; chain = realloc(chain, n * sizeof *chain); }
    int nc = 0; for (int c = to; c != -1 && nc < n; c = prev[c]) chain[nc++] = c;
    for (int i = 0; i < nc / 2; i++) { int t = chain[i]; chain[i] = chain[nc - 1 - i]; chain[nc - 1 - i] = t; }
    static float pts[4096][4]; int np = 0;
    pts[np][0] = (float)G.x; pts[np][1] = (float)G.y; pts[np][2] = (float)G.z; pts[np][3] = 0; np++;
    for (int j = 1; j < nc && np < 4095; j++) { float *q = at[chain[j]]; int dy = B[5 * chain[j - 1] + 4] - B[5 * chain[j] + 4]; pts[np][0] = q[0]; pts[np][1] = q[1]; pts[np][2] = q[2]; pts[np][3] = abs(dy) >= 512 ? (dy > 0 ? 1 : -1) : 0; np++; }
    pts[np][0] = (float)st->x; pts[np][1] = (float)st->y; pts[np][2] = (float)st->z; pts[np][3] = 0; np++;
    RT.n = 0; memset(RT.set, 0, sizeof RT.set); RT.high = climb > CLIMB; RT.straight = 0;
    rtAdd(pts[0][0], pts[0][1], pts[0][2], 0);
    for (int m = 1; m < np - 1; m++) {   /* points on a straight line with their neighbours go */
        float *P = RT.p[RT.n - 1], *Q = pts[m], *N = pts[m + 1];
        float cr = (Q[0] - P[0]) * (N[2] - P[2]) - (Q[2] - P[2]) * (N[0] - P[0]), len = hyp(N[0] - P[0], N[2] - P[2]); if (len == 0) len = 1;
        if (Q[3] != 0 || fabsf(cr) / len > 160) rtAdd(Q[0], Q[1], Q[2], Q[3]);
    }
    rtAdd(pts[np - 1][0], pts[np - 1][1], pts[np - 1][2], 0);
    for (int j = 0; j < nc; j++) { int r = LV.boxRoom[chain[j]]; if (r >= 0) setRoom(canon(r)); }
    setRoom(canon(st->room));
    RT.rooms = countSet();
    return 1;
}
static void routeDrawn(int i, const TrStep *st) {
    float Q[66][4]; int nq = 0; const TrStep *pv = i > 0 ? stepAt(LV.n, i - 1) : NULL;
    if (pv) { Q[nq][0] = (float)pv->x; Q[nq][1] = (float)pv->y; Q[nq][2] = (float)pv->z; Q[nq][3] = (float)pv->room; nq++; }
    for (int k = 0; k < st->nw && nq < 64; k++) { for (int c = 0; c < 4; c++) Q[nq][c] = (float)st->w[k][c]; nq++; }
    Q[nq][0] = (float)st->x; Q[nq][1] = (float)st->y; Q[nq][2] = (float)st->z; Q[nq][3] = (float)st->room; nq++;
    float best = 1e30f, bp[3] = { Q[0][0], Q[0][1], Q[0][2] }; int bk = 0;
    for (int k = 0; k < nq - 1; k++) {
        float *A_ = Q[k], *B = Q[k + 1], ex = B[0] - A_[0], ey = B[1] - A_[1], ez = B[2] - A_[2], l2 = ex * ex + 4 * ey * ey + ez * ez;
        float t = l2 ? ((G.x - A_[0]) * ex + 4 * (G.y - A_[1]) * ey + (G.z - A_[2]) * ez) / l2 : 0; t = t < 0 ? 0 : t > 1 ? 1 : t;
        float P[3] = { A_[0] + ex * t, A_[1] + ey * t, A_[2] + ez * t }, dx = P[0] - G.x, dy = 2 * (P[1] - G.y), dz = P[2] - G.z, d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < best) { best = d; bk = k; bp[0] = P[0]; bp[1] = P[1]; bp[2] = P[2]; }
    }
    if (nq == 1) bk = -1;
    RT.n = 0; memset(RT.set, 0, sizeof RT.set); RT.high = RT.straight = 0;
    rtAdd((float)G.x, (float)G.y, (float)G.z, 0); rtAdd(bp[0], bp[1], bp[2], 0); setRoom(canon(G.room));
    for (int j = bk + 1; j < nq; j++) { float *q = Q[j], *pr = RT.p[RT.n - 1], dy = pr[1] - q[1]; rtAdd(q[0], q[1], q[2], fabsf(dy) >= 512 ? (dy > 0 ? 1 : -1) : 0); setRoom(canon((int)q[3])); }
    RT.rooms = countSet();
}
static Route *route(void) {
    int i = shownStep(); if (i < 0 || !LV.loaded || !G.ok) return NULL; const TrStep *st = stepAt(LV.n, i);
    if (st->nw) { routeDrawn(i, st); return &RT; }
    int on = LV.nb ? boxAt(G.room, G.x, G.z) : -1;
    char key[64]; if (on >= 0) snprintf(key, sizeof key, "%d:%d:%d:b%d", LV.n, i, G.flip, on); else snprintf(key, sizeof key, "%d:%d:%d:%d:%d:%d", LV.n, i, G.flip, G.room, G.x >> 9, G.z >> 9);
    if (rtOk && !strcmp(key, rtKey)) { RT.p[0][0] = (float)G.x; RT.p[0][1] = (float)G.y; RT.p[0][2] = (float)G.z; return &RT; }
    int a = on >= 0 ? on : LV.nb ? boxNear(G.room, G.x, G.y, G.z) : -1, b = LV.nb ? boxNear(st->room, st->x, st->y, st->z) : -1;
    int ok = 0; if (a >= 0 && b >= 0 && a < LV.nb && b < LV.nb) ok = boxPath(a, b, CLIMB, st) || boxPath(a, b, 1000000000, st);
    if (!ok) routeStraight(st);
    snprintf(rtKey, sizeof rtKey, "%s", key); rtOk = 1; return &RT;
}

/* ================================================================== secrets (index.html's secretList) */
static void secretsGroup(void) {
    LV.nsg = 0; int pts[32][16], np[32]; memset(np, 0, sizeof np);
    for (int s = 0; s < LV.ns; s++) {
        const int32_t *q = LV.sec + 5 * s; int g = -1;
        for (int i = 0; i < LV.nsg && g < 0; i++) {
            if (LV.sg[i].i != q[0]) continue;
            for (int k = 0; k < np[i]; k++) { const int32_t *p = LV.sec + 5 * pts[i][k]; if (canon(p[1]) == canon(q[1]) || (hyp((float)(p[2] - q[2]), (float)(p[4] - q[4])) <= 3072 && abs(p[3] - q[3]) <= 2048)) { g = i; break; } }
        }
        if (g < 0) { if (LV.nsg >= 32) continue; g = LV.nsg++; LV.sg[g].i = q[0]; }
        if (np[g] < 16) pts[g][np[g]++] = s;
    }
    for (int i = 0; i < LV.nsg; i++) {
        float x = 0, y = 0, z = 0; for (int k = 0; k < np[i]; k++) { const int32_t *p = LV.sec + 5 * pts[i][k]; x += p[2]; y += p[3]; z += p[4]; }
        LV.sg[i].x = x / np[i]; LV.sg[i].y = y / np[i]; LV.sg[i].z = z / np[i]; LV.sg[i].room = LV.sec[5 * pts[i][0] + 1];
    }
    for (int i = 1; i < LV.nsg; i++) for (int k = i; k > 0 && (LV.sg[k - 1].i > LV.sg[k].i || (LV.sg[k - 1].i == LV.sg[k].i && LV.sg[k - 1].room > LV.sg[k].room)); k--) { __typeof__(LV.sg[0]) t = LV.sg[k]; LV.sg[k] = LV.sg[k - 1]; LV.sg[k - 1] = t; }
}
static int bits(unsigned v) { int c = 0; for (; v; v >>= 1) c += v & 1; return c; }

/* ================================================================== the screen */
/* a tap target, made while drawing */
typedef struct { int x, y, w, h, id, arg; } Hot;
static Hot HOT[48]; static int nHot;
static void hot(int x, int y, int w, int h, int id, int arg) { if (nHot < 48) { Hot *t = HOT + nHot++; t->x = x; t->y = y; t->w = w; t->h = h; t->id = id; t->arg = arg; } }
enum { H_SYS = 1, H_ZOOMIN, H_ZOOMOUT, H_TURN, H_CENTRE, H_PREV, H_NEXT, H_FOLD, H_SECRETS, H_CLOSE, H_SET, H_ACT, H_MAP };
static int page;                /* 0 the map, 1 SYSTEM, 2 secrets */
static struct { float dx, dz; int drag, x0, y0, moved; float sdx, sdz; uint32_t at; } VIEW;
static const LinkIn *IN;
static int mapY0 = 20, mapY1 = 160;

static void button(int x, int y, int w, int h, int on) {
    rectA(x, y, w, h, C_BTN, 230); frame(x, y, w, h, on ? C_GOLD : C_LINE);
}
/* a room's picture with the map's turn and zoom: each screen pixel looks up its texel (no divide in the loop) */
static void blitRoom(const Room *r, int k, float cxw, float czw, float sc, float rot, float ox, float oy, int alpha) {
    if (!LV.pxb) return; const int32_t *pc = r->pic + 3 * k; uint32_t off = pc[0]; int w = pc[1], h = pc[2];
    if (w <= 0 || h <= 0 || off + (uint32_t)w * h * 2 > LV.pxLen) return;
    const uint16_t *src = (const uint16_t *)((const uint8_t *)LV.pxb + off);
    float c = cosf(rot), s = sinf(rot), x0 = (float)RX(r), z1 = (float)(RZ(r) + RZS(r) * 1024);
    /* corners on the screen */
    float X[4] = { x0, x0 + RXS(r) * 1024.0f, x0 + RXS(r) * 1024.0f, x0 }, Z[4] = { z1, z1, (float)RZ(r), (float)RZ(r) }, lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
    for (int i = 0; i < 4; i++) { float dx = (X[i] - cxw) * sc, dy = -(Z[i] - czw) * sc, sx = ox + dx * c - dy * s, sy = oy + dx * s + dy * c; if (sx < lo[0]) lo[0] = sx; if (sx > hi[0]) hi[0] = sx; if (sy < lo[1]) lo[1] = sy; if (sy > hi[1]) hi[1] = sy; }
    int bx0 = (int)floorf(lo[0]), bx1 = (int)ceilf(hi[0]), by0 = (int)floorf(lo[1]), by1 = (int)ceilf(hi[1]);
    if (bx0 < clipX0) bx0 = clipX0; if (by0 < clipY0) by0 = clipY0; if (bx1 > clipX1) bx1 = clipX1; if (by1 > clipY1) by1 = clipY1;
    if (bx0 >= bx1 || by0 >= by1) return;
    /* texel (u, v) at screen (sx, sy): u = (x - x0) / 64, v = (z1 - z) / 64, x = cxw + (ex c + ey s) / sc, z = czw - (-ex s + ey c) / sc */
    float inv = 1.0f / (64 * sc);
    float du_dx = c * inv, du_dy = s * inv, dv_dx = -s * inv, dv_dy = c * inv;
    float u00 = (cxw - x0) / 64.0f, v00 = (z1 - czw) / 64.0f;
    for (int y = by0; y < by1; y++) {
        float ey = y + 0.5f - oy, ex = bx0 + 0.5f - ox, uf = u00 + ex * du_dx + ey * du_dy, vf = v00 + ex * dv_dx + ey * dv_dy;
        /* only the part of the row inside the picture: 0 <= u < w and 0 <= v < h, solved for the step along the row */
        float t0 = 0, t1 = (float)(bx1 - bx0);
        if (du_dx > 1e-6f) { t0 = fmaxf(t0, -uf / du_dx); t1 = fminf(t1, (w - uf) / du_dx); } else if (du_dx < -1e-6f) { t0 = fmaxf(t0, (w - uf) / du_dx); t1 = fminf(t1, -uf / du_dx); } else if (uf < 0 || uf >= w) continue;
        if (dv_dx > 1e-6f) { t0 = fmaxf(t0, -vf / dv_dx); t1 = fminf(t1, (h - vf) / dv_dx); } else if (dv_dx < -1e-6f) { t0 = fmaxf(t0, (h - vf) / dv_dx); t1 = fminf(t1, -vf / dv_dx); } else if (vf < 0 || vf >= h) continue;
        int xa = bx0 + (int)floorf(t0) - 1, xb = bx0 + (int)ceilf(t1) + 1; if (xa < bx0) xa = bx0; if (xb > bx1) xb = bx1; if (xa >= xb) continue;
        int32_t u = (int32_t)((uf + (xa - bx0) * du_dx) * 65536), v = (int32_t)((vf + (xa - bx0) * dv_dx) * 65536);
        int32_t uu = (int32_t)(du_dx * 65536), vv = (int32_t)(dv_dx * 65536);
        uint16_t *d = fb + y * LINK_W + xa;
        for (int x = xa; x < xb; x++, d++, u += uu, v += vv) {
            int iu = u >> 16, iv = v >> 16; if ((unsigned)iu >= (unsigned)w || (unsigned)iv >= (unsigned)h) continue;
            uint16_t p = src[iv * w + iu]; if (!p) continue;
            *d = alpha >= 256 ? p : mix(p, *d, alpha);
        }
    }
}
static void blitIcon(const Icon *ic, int cx, int cy, int size, int alpha) {
    int m = ic->w > ic->h ? ic->w : ic->h; if (m <= 0) return; float sc = (float)size / m; int w = (int)(ic->w * sc), h = (int)(ic->h * sc); if (w < 1) w = 1; if (h < 1) h = 1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint16_t p = ic->px[(int)(y / sc) * ic->w + (int)(x / sc)]; if (!p) continue;
        alpha >= 256 ? put(cx - w / 2 + x, cy - h / 2 + y, p) : putA(cx - w / 2 + x, cy - h / 2 + y, p, alpha);
    }
}
static const Icon *stepIcon(const TrStep *s) {
    if (!s || !s->nit) return NULL; int k = -1;
    for (int q = 0; q < s->nit; q++) if (s->it[q] >= 0 && s->it[q] < LV.ni && !LV.taken[s->it[q]]) { k = s->it[q]; break; }
    if (k < 0) k = s->it[0]; if (k < 0 || k >= LV.ni) return NULL;
    int t = LV.item[7 * k];
    if (t >= 137 && t <= 140) t = 129 + t - 137; else if (t >= 118 && t <= 121) t = 110 + t - 118; else if (t >= 122 && t <= 125) t = 110 + t - 122;
    return iconFor(LV.n, t);
}

static float mapRot(void) { if (!(CFG.rot || CFG.view) || !G.ok) return 0; return -(CFG.turnby == 0 && G.camOk ? G.cam : G.hd); }   /* the compass always turns */

/* the compass (index.html's drawCompass): no rooms, a ring turned like Turn the map, N E S W, Lara's arrow in the middle and a needle
   to the first point of the way more than a block off (so it follows the corners of the way), ▲/▼ when that point is a climb or drop,
   a pulsing ring at the spot; Next step off: no needle */
static void drawCompass(int y0, int H) {
    int W = LINK_W; float ox = W / 2.0f, oy = y0 + H / 2.0f, R = (W < H ? W : H) * 0.38f, rot = mapRot();
    disc(ox, oy, R * 1.08f, RGB(30, 22, 12), 220); ring(ox, oy, R * 1.08f, 2, C_GOLD, 220); ring(ox, oy, R * 0.78f, 1, C_GOLD2, 110);
    for (int t = 0; t < 24; t++) {
        if (t % 6 == 0) continue; float a = t * (float)M_PI / 12 + rot, l = t % 3 == 0 ? R * 0.1f : R * 0.05f, sa = sinf(a), ca = cosf(a);
        lineW(ox + sa * R * 1.05f, oy - ca * R * 1.05f, ox + sa * (R * 1.05f - l), oy - ca * (R * 1.05f - l), t % 3 == 0 ? 2 : 1, C_GOLD, 160, 0, 0, NULL);
    }
    static const char *NESW[4] = { "N", "E", "S", "W" };
    for (int i = 0; i < 4; i++) { float a = i * (float)M_PI / 2 + rot; textC(NESW[i], (int)(ox + sinf(a) * R * 0.92f), (int)(oy - cosf(a) * R * 0.92f) - 7, i ? C_GOLD2 : C_GOLD); }
    int si = shownStep(); const TrStep *st = si >= 0 && CFG.step ? stepAt(LV.n, si) : NULL; Route *rt = st ? route() : NULL;
    if (st) {
        float q[4] = { (float)st->x, (float)st->y, (float)st->z, 0 };
        if (rt) for (int j = 1; j < rt->n; j++) if (hyp(rt->p[j][0] - G.x, rt->p[j][2] - G.z) > 1024 || fabsf(rt->p[j][1] - G.y) > 768) { memcpy(q, rt->p[j], sizeof q); break; }
        int here = hyp((float)(st->x - G.x), (float)(st->z - G.z)) < 1536 && abs(st->y - G.y) < 768;
        if (here) { float ph = (IN->nowMs % 1400) / 1400.0f; ring(ox, oy, R * (0.25f + ph * 0.5f), 2, C_GREEN, (int)(256 * (1 - ph))); }
        else {
            float a = atan2f(q[0] - G.x, q[2] - G.z) + rot, ca = cosf(a), sa = sinf(a);
            float P[14] = { 0, -0.74f, 0.1f, -0.5f, 0.035f, -0.5f, 0.035f, -0.22f, -0.035f, -0.22f, -0.035f, -0.5f, -0.1f, -0.5f }, Q[14];
            for (int i = 0; i < 7; i++) { float px = P[2 * i] * R, py = P[2 * i + 1] * R; Q[2 * i] = ox + px * ca - py * sa; Q[2 * i + 1] = oy + px * sa + py * ca; }
            poly(Q, 7, C_GREEN, 256); polyLine(Q, 7, RGB(0x0c, 0x1a, 0x0e));
            float dy = (float)G.y - q[1]; int v = q[3] != 0 ? (q[3] > 0 ? 1 : -1) : fabsf(dy) >= 768 ? (dy > 0 ? 1 : -1) : 0;
            if (v) { float mx = ox + sa * R * 0.62f + R * 0.16f, my = oy - ca * R * 0.62f, up = v > 0 ? -1 : 1, t[6] = { mx, my + 4 * up, mx - 4, my - 3 * up, mx + 4, my - 3 * up }; poly(t, 3, C_GREEN, 256); }
        }
    } else text(si < 0 ? "No story step here" : "Next step is off", (int)(ox - textW(si < 0 ? "No story step here" : "Next step is off") / 2), (int)(oy + R * 1.08f + 3), C_DIM);
    /* Lara, turned the way she faces */
    float a = G.hd + rot, ca = cosf(a), sa = sinf(a), P[8] = { 0, -10, 7, 8, 0, 4, -7, 8 }, Q[8];
    for (int i = 0; i < 4; i++) { Q[2 * i] = ox + P[2 * i] * ca - P[2 * i + 1] * sa; Q[2 * i + 1] = oy + P[2 * i] * sa + P[2 * i + 1] * ca; }
    poly(Q, 4, RGB(0xff, 0xf3, 0xd0), 256); polyLine(Q, 4, RGB(0x5a, 0x38, 0x10));
}
static int roomShown(int i, int cur) { if (!CFG.reveal) return 1; int c = canon(i); return seen(c) || c == cur || inSet(c); }
static int cutFor(const Room *r) { int k = -1; for (int i = 0; i < r->nc; i++) if (r->cut[i] <= G.y - 1536) k = i; return k; }

static void drawMap(void) {
    int W = LINK_W, y0 = mapY0, y1 = mapY1, H = y1 - y0;
    clip(0, y0, W, y1);
    /* behind the map: the level's floor texture at twice its size under a dark wash (index.html's mapBackdrop), made once a level
       (the 3DS has no divide instruction, so not a modulo per pixel each time) */
    static uint16_t back[LINK_W * LINK_H]; static int backFor = -2;
    if (backFor != LV.n) {
        backFor = LV.n; const int32_t *bg = LV.bg;
        if (LV.pxb && bg && bg[1] > 0 && bg[2] > 0 && bg[0] + bg[1] * bg[2] * 2 <= (int)LV.pxLen) {
            const uint16_t *t = (const uint16_t *)((const uint8_t *)LV.pxb + bg[0]); int tw = bg[1], th = bg[2];
            for (int y = 0; y < LINK_H; y++) for (int x = 0; x < W; x++) back[y * W + x] = mix(t[((y >> 1) % th) * tw + (x >> 1) % tw], C_BG, 33);
        } else for (int i = 0; i < LINK_W * LINK_H; i++) back[i] = C_BG;
    }
    memcpy(fb + y0 * W, back + y0 * W, (size_t)H * W * 2);
    if (!G.ok || !LV.loaded) { noclip(); return; }
    if (CFG.view) { drawCompass(y0, H); noclip(); return; }   /* no map buttons over the compass */
    float sc = CFG.zoom / 1024.0f, ox = W / 2.0f, oy = y0 + H / 2.0f, rot = mapRot(), cxw = G.x + VIEW.dx, czw = G.z + VIEW.dz, c = cosf(rot), s = sinf(rot);
    int cur = canon(G.room), si = shownStep(); Route *rt = si >= 0 ? route() : NULL; if (!rt) memset(RT.set, 0, sizeof RT.set);
#define SCR(X, Z, SX, SY) do { float dx_ = ((X) - cxw) * sc, dy_ = -((Z) - czw) * sc; SX = ox + dx_ * c - dy_ * s; SY = oy + dx_ * s + dy_ * c; } while (0)
    /* rooms: lower ones first; one of each normal/alternate pair; flipped rooms drawn with their alternate's picture */
    static int list[1024]; int nl = 0; float reach = hyp((float)W, (float)H) / sc * 0.75f;
    for (int i = 0; i < LV.nr && nl < 1024; i++) {
        if (LV.altOf[i] >= 0 || !roomShown(i, cur)) continue;
        int sh = G.flip && RALT(LV.R + i) >= 0 && RALT(LV.R + i) < LV.nr ? RALT(LV.R + i) : i; Room *r = LV.R + sh;
        if (fabsf(RX(r) + RXS(r) * 512 - cxw) > reach + RXS(r) * 512 || fabsf(RZ(r) + RZS(r) * 512 - czw) > reach + RZS(r) * 512) continue;
        if (i != G.room && RYB(r) < G.y - 2600) continue;
        list[nl++] = i;
    }
    for (int i = 1; i < nl; i++) for (int k = i; k > 0; k--) {
        int a = list[k - 1], b = list[k]; int sa = G.flip && RALT(LV.R + a) >= 0 ? RALT(LV.R + a) : a, sb = G.flip && RALT(LV.R + b) >= 0 ? RALT(LV.R + b) : b;
        if (RYB(LV.R + sa) < RYB(LV.R + sb)) { list[k - 1] = b; list[k] = a; } else break;
    }
    for (int q = 0; q < nl; q++) {
        int i = list[q], sh = G.flip && RALT(LV.R + i) >= 0 && RALT(LV.R + i) < LV.nr ? RALT(LV.R + i) : i; Room *r = LV.R + sh;
        if (RYT(r) > G.y + 1024 && i != G.room) {   /* wholly below Lara's floor: faint, outlined */
            blitRoom(r, 0, cxw, czw, sc, rot, ox, oy, 56);
            if (RXS(r) > 2 && RZS(r) > 2) {
                float p[8]; float xa = (float)RX(r) + 1024, xb = (float)RX(r) + (RXS(r) - 1) * 1024, za = (float)RZ(r) + 1024, zb = (float)RZ(r) + (RZS(r) - 1) * 1024;
                SCR(xa, za, p[0], p[1]); SCR(xb, za, p[2], p[3]); SCR(xb, zb, p[4], p[5]); SCR(xa, zb, p[6], p[7]);
                for (int e = 0; e < 4; e++) lineW(p[2 * e], p[2 * e + 1], p[(2 * e + 2) % 8], p[(2 * e + 3) % 8], 1, C_GOLD, 77, 0, 0, NULL);
            }
            continue;
        }
        int k = cutFor(r); blitRoom(r, k + 1, cxw, czw, sc, rot, ox, oy, 256);
    }
    /* Lara's room: outlined inside its outer ring of wall sectors */
    if (G.room >= 0 && G.room < LV.nr) {
        Room *r = LV.R + (G.flip && RALT(LV.R + G.room) >= 0 && RALT(LV.R + G.room) < LV.nr ? RALT(LV.R + G.room) : G.room);
        if (RXS(r) > 2 && RZS(r) > 2) {
            float p[8]; float xa = (float)RX(r) + 1024, xb = (float)RX(r) + (RXS(r) - 1) * 1024, za = (float)RZ(r) + 1024, zb = (float)RZ(r) + (RZS(r) - 1) * 1024;
            SCR(xa, za, p[0], p[1]); SCR(xb, za, p[2], p[3]); SCR(xb, zb, p[4], p[5]); SCR(xa, zb, p[6], p[7]);
            for (int e = 0; e < 4; e++) lineW(p[2 * e], p[2 * e + 1], p[(2 * e + 2) % 8], p[(2 * e + 3) % 8], 2, C_GOLD, 180, 0, 0, NULL);
        }
    }
    /* the way to the shown step */
    if (rt && CFG.step && rt->n > 1) {
        float ph = 0, px, py; SCR((float)G.x, (float)G.z, px, py);
        for (int j = 1; j < rt->n; j++) { float qx, qy; SCR(rt->p[j][0], rt->p[j][2], qx, qy); lineW(px, py, qx, qy, 2, C_GREEN, 230, rt->straight ? 2 : 6, rt->straight ? 5 : 4, &ph); px = qx; py = qy; }
        for (int j = 1; j < rt->n - 1; j++) if (rt->p[j][3] != 0) {
            float qx, qy; SCR(rt->p[j][0], rt->p[j][2], qx, qy); float up = rt->p[j][3] > 0 ? -1 : 1, t[6] = { qx, qy + 4 * up, qx - 4, qy - 3 * up, qx + 4, qy - 3 * up };
            poly(t, 3, C_GREEN, 256);
        }
    }
    float ms = CFG.zoom * 0.75f; if (ms < 10) ms = 10; if (ms > 20) ms = 20;
    /* secrets: found ones faded */
    if (CFG.secrets) for (int i = 0; i < LV.nsg; i++) {
        if (!roomShown(LV.sg[i].room, cur)) continue; float x, y; SCR(LV.sg[i].x, LV.sg[i].z, x, y);
        if (x < -20 || y < y0 - 20 || x > W + 20 || y > y1 + 20) continue;
        int got = (G.secrets >> LV.sg[i].i) & 1; star(x, y, ms * 0.4f, RGB(255, 215, 120), got ? 64 : 218);
    }
    /* exits */
    for (int e = 0; e < LV.ne; e++) {
        const int32_t *q = LV.exit + 4 * e; if (!roomShown(q[0], cur)) continue; float x, y; SCR((float)q[1], (float)q[3], x, y);
        if (x < -20 || y < y0 - 20 || x > W + 20 || y > y1 + 20) continue;
        frame((int)(x - ms * 0.35f), (int)(y - ms * 0.45f), (int)(ms * 0.7f), (int)(ms * 0.9f), C_GOLD);
    }
    /* pickups */
    if (CFG.pickups) for (int i = 0; i < LV.ni; i++) {
        const int32_t *it = LV.item + 7 * i; if (!it[5] || !roomShown(it[1], cur)) continue; int t = it[0];
        if (!IN(KEYT, t) && !IN(SUPT, t)) continue;
        float x, y; SCR((float)it[2], (float)it[4], x, y); if (x < -20 || y < y0 - 20 || x > W + 20 || y > y1 + 20) continue;
        int gone = LV.taken[i]; const Icon *ic = iconFor(LV.n, t);
        if (ic) blitIcon(ic, (int)x, (int)y, (int)ms, gone ? 72 : 256); else disc(x, y, 3, C_GOLD, gone ? 72 : 256);
    }
    /* the step's spot: a pulsing ring, or an arrow at the edge pointing to it */
    if (si >= 0 && CFG.step) {
        const TrStep *st = stepAt(LV.n, si); float sx, sy; SCR((float)st->x, (float)st->z, sx, sy); float ph = (IN->nowMs % 1400) / 1400.0f;
        if (sx >= 0 && sy >= y0 && sx <= W && sy <= y1) { ring(sx, sy, 6 + ph * 12, 2, C_GREEN, (int)(256 * (1 - ph))); disc(sx, sy, 3.5f, C_GREEN, 256); }
        else {
            float ax = sx - ox, ay = sy - oy, a = atan2f(ay, ax), m = fminf((W / 2.0f - 16) / fmaxf(fabsf(cosf(a)), 1e-6f), (H / 2.0f - 14) / fmaxf(fabsf(sinf(a)), 1e-6f));
            float ex = ox + cosf(a) * m, ey = oy + sinf(a) * m, ca = cosf(a), sa = sinf(a), P[8] = { 10, 0, -6, -7, -2, 0, -6, 7 }, Q[8];
            for (int i = 0; i < 4; i++) { Q[2 * i] = ex + P[2 * i] * ca - P[2 * i + 1] * sa; Q[2 * i + 1] = ey + P[2 * i] * sa + P[2 * i + 1] * ca; }
            poly(Q, 4, C_GREEN, 256);
        }
    }
    /* Lara */
    {
        float lx, ly; SCR((float)G.x, (float)G.z, lx, ly); float a = G.hd + rot, ca = cosf(a), sa = sinf(a), P[8] = { 0, -10, 7, 8, 0, 4, -7, 8 }, Q[8];
        for (int i = 0; i < 4; i++) { Q[2 * i] = lx + P[2 * i] * ca - P[2 * i + 1] * sa; Q[2 * i + 1] = ly + P[2 * i] * sa + P[2 * i + 1] * ca; }
        poly(Q, 4, RGB(0xff, 0xf3, 0xd0), 256); polyLine(Q, 4, RGB(0x5a, 0x38, 0x10));
    }
    /* north, when the map turns */
    if (CFG.rot) {
        float nx = W - 52.0f, ny = y1 - 12.0f, P[2] = { 0, -9 }; float tx = nx + P[0] * c - P[1] * s, ty = ny + P[0] * s + P[1] * c;
        lineW(nx, ny, tx, ty, 2, C_GOLD, 220, 0, 0, NULL); textC("N", (int)(nx + (tx - nx) * 1.9f), (int)(ny + (ty - ny) * 1.9f) - 7, C_GOLD);
    }
#undef SCR
    /* the buttons down the right */
    int bx = W - 28, by = y0 + 4;
    button(bx, by, 24, 22, 0); rect(bx + 6, by + 10, 12, 2, C_INK); rect(bx + 11, by + 5, 2, 12, C_INK); hot(bx - 4, by - 2, 32, 26, H_ZOOMIN, 0); by += 26;
    button(bx, by, 24, 22, 0); rect(bx + 6, by + 10, 12, 2, C_INK); hot(bx - 4, by - 2, 32, 26, H_ZOOMOUT, 0); by += 26;
    button(bx, by, 24, 22, CFG.rot); ring(bx + 12.0f, by + 11.0f, 6, 1.6f, C_INK, 256); { float t[6] = { bx + 18.0f, by + 7.0f, bx + 22.0f, by + 12.0f, bx + 15.0f, by + 12.0f }; poly(t, 3, C_INK, 256); }
    hot(bx - 4, by - 2, 32, 26, H_TURN, 0); by += 26;
    if (VIEW.dx != 0 || VIEW.dz != 0) { button(bx, by, 24, 22, 0); ring(bx + 12.0f, by + 11.0f, 6, 1.6f, C_INK, 256); disc(bx + 12.0f, by + 11.0f, 2, C_INK, 256); hot(bx - 4, by - 2, 32, 26, H_CENTRE, 0); }
    hot(0, y0, W - 32, H, H_MAP, 0);
    noclip();
}

/* the card over the map when there's nothing to draw */
static void note(const char *title, const char *small) {
    int w = 240, x = (LINK_W - w) / 2, lines = small ? wrap(small, 0, 0, w - 20, 4, 0, 0) : 0, h = 22 + lines * LINEH + 12, y = mapY0 + (mapY1 - mapY0 - h) / 2;
    rectA(x, y, w, h, C_PANEL, 220); frame(x, y, w, h, C_LINE);
    int tw = textW(title); textGold(title, x + (w - tw) / 2, y + 6);
    if (small) wrap(small, x + 10, y + 22, w - 20, 4, C_DIM, 1);
}

static void drawHeader(void) {
    rect(0, 0, LINK_W, mapY0, RGB(6, 5, 4)); rect(0, mapY0 - 1, LINK_W, 1, C_LINE);
    char t[48]; const char *nm = G.level >= 0 ? levelName(G.level) : NULL;
    snprintf(t, sizeof t, "%s", page == 1 ? "System" : page == 2 ? "Secrets" : nm ? nm : "Tomb Raider Link"); for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    clip(0, 0, LINK_W - 90, mapY0); textGold(t, 6, 3); noclip();
    /* the gear (SYSTEM) */
    int gx = LINK_W - 22; button(gx, 2, 20, 16, page == 1); ring(gx + 10.0f, 10.0f, 3.5f, 2, C_INK, 256);
    for (int i = 0; i < 8; i++) { float a = i * (float)M_PI / 4; disc(gx + 10 + cosf(a) * 5.5f, 10 + sinf(a) * 5.5f, 1.2f, C_INK, 256); }
    hot(gx - 6, 0, 28, mapY0, H_SYS, 0);
    /* secrets found / in the level */
    if (LV.loaded && G.ok && !G.demo && LV.nsg) {
        snprintf(t, sizeof t, "%d / %d", bits(G.secrets), LV.nsg); int w = textW(t) + 20, x = gx - 6 - w;
        frameA(x, 2, w, 16, C_LINE, 256); star(x + 8.0f, 10.0f, 5, RGB(255, 215, 120), 256); text(t, x + 15, 2, C_DIM);
        hot(x, 0, w, mapY0, H_SECRETS, 0);
    }
}

static void drawHint(int draw) {
    int si = shownStep(), W = LINK_W; char t[300] = "", way[120] = "", lab[16] = ""; const TrStep *st = NULL; int nostep = 0, done = 0;
    if (REV < 0) snprintf(t, sizeof t, "Waiting for Tomb Raider...");
    else if (G.level == 20) snprintf(t, sizeof t, "Title screen. Start or load a game.");
    else if (G.level >= 16 && G.level <= 19) snprintf(t, sizeof t, "Cut scene.");
    else if (!G.ok) snprintf(t, sizeof t, "Waiting for Lara...");
    else if (G.demo) { snprintf(lab, sizeof lab, "DEMO"); snprintf(t, sizeof t, "A demo is playing. Story steps wait for your own game."); }
    else if (G.ending) { snprintf(lab, sizeof lab, "%s", LV.n == 0 ? "TRAINING" : ""); snprintf(t, sizeof t, "Level complete."); done = 1; }
    else if (si < 0) snprintf(t, sizeof t, "No story steps for this level.");
    else if (!CFG.step) nostep = 1;
    else {
        st = stepAt(LV.n, si); done = stepDone(LV.n, si); snprintf(lab, sizeof lab, "%s", LV.n == 0 ? "TRAINING" : "");
        if (CFG.hinttext) snprintf(t, sizeof t, "%s", st->text[0] == '?' ? st->text + 1 : st->text);
        else snprintf(t, sizeof t, "Step %d of %d", si + 1, HINTS[LV.n].n);   /* Hint text off: only which step it is */
        wayTo(way, sizeof way, st->x, st->y, st->z, 1.5f, "You are here.");
        Route *rt = route(); if (rt && rt->rooms > 1) { int k = (int)strlen(way); snprintf(way + k, sizeof way - k, " %c %d room%s", haveFont ? 0x7f : '-', rt->rooms - 1, rt->rooms > 2 ? "s" : ""); }
        if (rt && strncmp(way, "You", 3) && (rt->high || rt->straight)) { int k = (int)strlen(way); snprintf(way + k, sizeof way - k, " %c %s", haveFont ? 0x7f : '-', rt->high ? "high climbs" : "no walkable way"); }
    }
    /* what Lara carries */
    char held[160] = ""; if (G.ok && !G.demo && G.nheld) { int k = snprintf(held, sizeof held, "Carrying: "); for (int i = 0; i < G.nheld && k < (int)sizeof held - 4; i++) { k += snprintf(held + k, sizeof held - k, "%s%s", i ? ", " : "", itemName(LV.n, G.held[i][0])); if (G.held[i][1] > 1) k += snprintf(held + k, sizeof held - k, " x%d", G.held[i][1]); } }
    if (nostep && !held[0]) { mapY1 = LINK_H; return; }
    /* the panel's height from what it holds */
    int tx = 22, tw = W - 44, icw = 0; const Icon *ic = st ? stepIcon(st) : NULL; if (ic) icw = 26;
    int nlines = nostep ? 0 : CFG.fold ? 1 : wrap(t, 0, 0, tw - icw, 5, 0, 0);
    int h = (nostep ? 0 : 14 + nlines * LINEH) + (way[0] && !CFG.fold ? LINEH : 0) + (held[0] && !CFG.fold ? LINEH : 0) + 6;
    if (nostep) h = LINEH + 8;
    int y = LINK_H - h; mapY1 = y;
    if (!draw) return;
    rect(0, y, W, h, C_PANEL);   /* below the map, as on the phone (and the buffer keeps the last frame: never blend here) */ rect(0, y, W, 1, C_LINE);
    int yy = y + 2;
    if (!nostep) {
        /* the header: label, a dot per step (current bright, done filled, the one viewed ringed) */
        int dx = 6; if (lab[0]) { text(lab, dx, yy - 1, C_GOLD); dx += textW(lab) + 8; }
        if (st) { int H = HINTS[LV.n].n; for (int d = 0; d < H; d++) { int x = dx + d * 10; float cx = x + 3.5f, cy = yy + 6.0f;
            if (d == STEP.cur) disc(cx, cy, 4, C_GOLD, 256); else if (stepDone(LV.n, d)) disc(cx, cy, 3.5f, C_GOLD2, 256); else ring(cx, cy, 3.2f, 1, C_GOLD2, 256);
            if (d == si && STEP.view >= 0) ring(cx, cy, 5.5f, 1, C_INK, 256); } }
        text(CFG.fold ? "+" : "-", W - 14, yy - 2, C_DIM);
        hot(0, y, W, 14, H_FOLD, 0);
        yy += 13;
        int x0 = tx; if (ic) { blitIcon(ic, tx + 11, yy + (nlines * LINEH) / 2, 22, 256); x0 += icw; }
        if (done) { tick(x0, yy + 2, C_GREEN); }
        int tx0 = x0 + (done ? 13 : 0);
        if (CFG.fold) { clip(0, yy, W - 22, yy + LINEH); text(t, tx0, yy, done ? C_DIM : C_INK); noclip(); }
        else wrap(t, tx0, yy, tw - icw - (done ? 13 : 0), 5, done ? C_DIM : C_INK, 1);
        yy += nlines * LINEH;
        if (st) {   /* previous and next step */
            int mid = y + 14 + (h - 14) / 2;
            float l[6] = { 6, (float)mid, 14, (float)mid - 6, 14, (float)mid + 6 }, r[6] = { W - 6.0f, (float)mid, W - 14.0f, (float)mid - 6, W - 14.0f, (float)mid + 6 };
            poly(l, 3, si > 0 ? C_GOLD2 : C_LINE, 256); poly(r, 3, si < HINTS[LV.n].n - 1 ? C_GOLD2 : C_LINE, 256);
            hot(0, y + 14, 22, h - 14, H_PREV, 0); hot(W - 22, y + 14, 22, h - 14, H_NEXT, 0);
        }
    }
    if (way[0] && !CFG.fold) { clip(0, yy, W - 22, yy + LINEH); text(way, tx, yy, C_DIM); noclip(); yy += LINEH; }
    if (held[0] && (!CFG.fold || nostep)) { clip(0, yy, W - 6, yy + LINEH); text(held, nostep ? 6 : tx, yy + (nostep ? 3 : 0), C_INK); noclip(); }
}

/* SYSTEM: the states, frame skip, the timings and the map's settings */
/* as index.html's Settings since Link 0.14.0: the map's rows first, then the hints; inv: the first choice is the value 0; map: only with
   the map (hidden with the compass); the rows are 16 pixels to fit the 3DS's 240 */
static const struct { const char *label; int *v; const char *a, *b; int inv, map; } SW[] = {
    { "Show", &CFG.view, "MAP", "COMPASS", 1, 0 }, { "Turn the map", &CFG.rot, "ON", "OFF", 0, 1 }, { "Up is", &CFG.turnby, "CAMERA", "LARA", 1, 0 },
    { "Reveal as you explore", &CFG.reveal, "ON", "OFF", 0, 1 }, { "Next step", &CFG.step, "ON", "OFF", 0, 0 }, { "Hint text", &CFG.hinttext, "ON", "OFF", 0, 0 },
    { "Pickups", &CFG.pickups, "ON", "OFF", 0, 1 }, { "Secrets on the map", &CFG.secrets, "ON", "OFF", 0, 1 },
};
static void seg(int x, int y, const char *a, const char *b, int first, int id, int arg) {
    int wa = textW(a) + 12, wb = textW(b) + 12;
    frame(x, y, wa + wb, 16, C_LINE);
    rect(x + 1, y + 1, wa - 1, 14, first ? C_GOLD : C_BTN); rect(x + wa, y + 1, wb - 1, 14, first ? C_BTN : C_GOLD);
    text(a, x + 6, y + 1, first ? RGB(0x1a, 0x12, 0x06) : C_DIM); text(b, x + wa + 6, y + 1, first ? C_DIM : RGB(0x1a, 0x12, 0x06));
    hot(x, y - 2, wa + wb, 20, id, arg);
}
static void pageSys(void) {
    rect(0, mapY0, LINK_W, LINK_H - mapY0, C_PANEL);
    int y = mapY0 + 3; char t[96];
    snprintf(t, sizeof t, "%d fps %c emu %d.%d ms %c max %d.%d", IN->fps, haveFont ? 0x7f : '-', IN->msEmu / 10, IN->msEmu % 10, haveFont ? 0x7f : '-', IN->msMax / 10, IN->msMax % 10);
    text(t, 8, y, C_DIM); y += 17;
    /* the save states: SAVE and LOAD per slot, with what the slot holds */
    for (int s = 0; s < 2; s++, y += 21) {
        snprintf(t, sizeof t, "SLOT %d", s + 1); text(t, 8, y, C_GOLD);
        button(84, y - 1, 62, 18, 0); textC("SAVE", 115, y, C_INK); hot(82, y - 2, 66, 21, H_ACT, s ? LINK_SAVE1 : LINK_SAVE0);
        button(152, y - 1, 62, 18, 0); textC("LOAD", 183, y, C_INK); hot(150, y - 2, 66, 21, H_ACT, s ? LINK_LOAD1 : LINK_LOAD0);
        text(IN->state[s] ? IN->state[s] : "-", 222, y, C_DIM);
    }
    y += 2;
    snprintf(t, sizeof t, "SKIP %d", IN->frameskip); button(8, y, 72, 18, IN->frameskip > 0); textC(t, 44, y, C_INK); hot(6, y - 2, 76, 22, H_ACT, LINK_FRAMESKIP);
    button(86, y, 120, 18, IN->old3ds); textC(IN->old3ds ? "OLD 3DS ON" : "OLD 3DS OFF", 146, y, C_INK); hot(84, y - 2, 124, 22, H_ACT, LINK_OLD3DS);
    button(LINK_W - 72, y, 64, 18, 0); textC("QUIT", LINK_W - 40, y, C_RED); hot(LINK_W - 74, y - 2, 68, 22, H_ACT, LINK_QUIT);
    y += 23; rect(8, y, LINK_W - 16, 1, C_LINE); y += 4;
    for (int i = 0; i < (int)(sizeof SW / sizeof SW[0]); i++) {
        if (SW[i].map && CFG.view) continue;                       /* map-only rows hide with the compass */
        if (SW[i].v == &CFG.turnby && !CFG.rot && !CFG.view) continue;   /* "Up is" only matters while the map or compass turns */
        text(SW[i].label, 8, y, C_DIM); int on = SW[i].inv ? *SW[i].v == 0 : *SW[i].v != 0;
        int w = textW(SW[i].a) + textW(SW[i].b) + 24; seg(LINK_W - 8 - w, y, SW[i].a, SW[i].b, on, H_SET, i); y += 16;
    }
    if (IN->msg) { int w = textW(IN->msg) + 16; rectA((LINK_W - w) / 2, LINK_H - 22, w, 18, C_BTN, 240); frame((LINK_W - w) / 2, LINK_H - 22, w, 18, C_GOLD); textC(IN->msg, LINK_W / 2, LINK_H - 21, C_GOLD); }
    else if (!pakF) text("No tomb-raider-link.pak beside the game: no map pictures.", 8, LINK_H - 18, C_RED);
}
static void pageSecrets(void) {
    rect(0, mapY0, LINK_W, LINK_H - mapY0, C_PANEL);
    int y = mapY0 + 4; char t[96]; const char *nm = levelName(LV.n);
    snprintf(t, sizeof t, "%s %c %d of %d found", nm ? nm : "", haveFont ? 0x7f : '-', bits(G.secrets), LV.nsg); text(t, 8, y, C_DIM); y += 20;
    for (int i = 0; i < LV.nsg && y < LINK_H - LINEH; i++) {
        int got = (G.secrets >> LV.sg[i].i) & 1; char w[80];
        wayTo(w, sizeof w, (int)LV.sg[i].x, (int)LV.sg[i].y, (int)LV.sg[i].z, 0.5f, "right here");
        star(14, y + 7.0f, 6, RGB(255, 215, 120), got ? 80 : 256);
        snprintf(t, sizeof t, "%d", i + 1); text(t, 24, y, C_GOLD);
        if (got) { text("Found", 42, y, C_GREEN); tick(LINK_W - 18, y + 1, C_GREEN); } else text(w, 42, y, C_INK);
        y += LINEH + 3;
    }
}

/* ================================================================== the outside */
void link_init(const char *dir) {
    snprintf(DIR, sizeof DIR, "%s", dir); pakOpen(); pakLoadCommon(); cfgLoad();
}
void link_draw(uint16_t *px, const LinkIn *in) {
    fb = px; IN = in; RAM = in->ram; nHot = 0; noclip();
    update(in->nowMs);
    if (VIEW.dx != 0 || VIEW.dz != 0) { if (!VIEW.drag && in->nowMs - VIEW.at > 15000) VIEW.dx = VIEW.dz = 0; }   /* the map goes back to Lara */
    if (page == 2 && !(LV.loaded && G.ok && LV.nsg)) page = 0;
    mapY1 = LINK_H;
    if (page == 0) {
        /* the hint panel's height decides where the map ends: measure it, draw the map above, then the panel */
        drawHint(0); drawMap(); drawHint(1);
        /* the Greatest Hits discs start in a menu of demos, where the game isn't running yet: so no "unknown release" verdict here */
        if (REV < 0) note("Waiting for Tomb Raider", G.unknown ? "On a Greatest Hits disc, pick Tomb Raider in its menu. This reads the US game: v1.0, v1.1, Rev 5 and Rev 6." : NULL);
        else if (G.level == 20) note("Title screen", "Start or load a game.");
        else if (G.ok && !LV.loaded) note("No map", "tomb-raider-link.pak isn't beside the game. Make it with tools/mk_pak.js from your disc.");
    } else if (page == 1) pageSys();
    else pageSecrets();
    drawHeader();
}
int link_touch(int phase, int x, int y, const LinkIn *in) {
    if (phase == TOUCH_DOWN) {
        VIEW.x0 = x; VIEW.y0 = y; VIEW.moved = 0; VIEW.drag = 0; VIEW.sdx = VIEW.dx; VIEW.sdz = VIEW.dz;
        if (page == 0 && y >= mapY0 && y < mapY1 && x < LINK_W - 32) VIEW.drag = 1;
        return LINK_NONE;
    }
    if (phase == TOUCH_HELD) {
        if (abs(x - VIEW.x0) + abs(y - VIEW.y0) > 6) VIEW.moved = 1;
        if (VIEW.drag && VIEW.moved && G.ok) {   /* drag the map: the screen's movement turned back into the world's */
            float rot = mapRot(), c = cosf(rot), s = sinf(rot), ex = (float)(x - VIEW.x0), ey = (float)(y - VIEW.y0), sc = CFG.zoom / 1024.0f;
            float dx = (ex * c + ey * s) / sc, dy = (-ex * s + ey * c) / sc;
            VIEW.dx = VIEW.sdx - dx; VIEW.dz = VIEW.sdz + dy; VIEW.at = in->nowMs;
        }
        return LINK_NONE;
    }
    /* up: a tap if it hardly moved */
    int wasDrag = VIEW.drag && VIEW.moved; VIEW.drag = 0; if (wasDrag) return LINK_NONE;
    x = VIEW.x0; y = VIEW.y0;
    for (int i = nHot - 1; i >= 0; i--) {
        Hot *h = HOT + i; if (x < h->x || y < h->y || x >= h->x + h->w || y >= h->y + h->h) continue;
        switch (h->id) {
        case H_SYS: page = page == 1 ? 0 : 1; return LINK_NONE;
        case H_SECRETS: page = page == 2 ? 0 : 2; return LINK_NONE;
        case H_ZOOMIN: CFG.zoom = (int)lroundf(CFG.zoom * 1.25f); if (CFG.zoom > 48) CFG.zoom = 48; cfgSave(); return LINK_NONE;
        case H_ZOOMOUT: CFG.zoom = (int)lroundf(CFG.zoom * 0.8f); if (CFG.zoom < 6) CFG.zoom = 6; cfgSave(); return LINK_NONE;
        case H_TURN: CFG.rot = !CFG.rot; cfgSave(); return LINK_NONE;
        case H_CENTRE: VIEW.dx = VIEW.dz = 0; return LINK_NONE;
        case H_PREV: stepNav(-1); return LINK_NONE;
        case H_NEXT: stepNav(1); return LINK_NONE;
        case H_FOLD: CFG.fold = !CFG.fold; cfgSave(); return LINK_NONE;
        case H_SET: { int k = h->arg; *SW[k].v = !*SW[k].v; cfgSave(); rtOk = 0; return LINK_NONE; }
        case H_ACT: return h->arg;
        case H_MAP: return LINK_NONE;
        }
    }
    if (page == 2) page = 0;
    return LINK_NONE;
}
int link_interval(void) { return page == 0 ? 6 : 15; }
void link_flush(void) { if (cfgDirty) cfgWrite(); }
void link_status(char *buf, int n) {
    int si = shownStep();
    snprintf(buf, n, "rev %d level %d ok %d demo %d room %d at %d,%d,%d step %d/%d %s", REV, G.level, G.ok, G.demo, G.room, G.x, G.y, G.z, si, LV.n >= 0 ? HINTS[LV.n].n : 0, LV.loaded ? "map" : "no map");
}

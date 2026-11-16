/* linkhost.c - the 3DS Link screen on the PC from a 2 MB RAM dump (Hint states/states/L<n>_S<k>.bin), for quick looks without
 * Azahar. Built and run by test/linkhost.sh.
 *   linkhost DIR OUT.ppm RAM.bin [RAM2.bin ...] [tap X Y] [drag X0 Y0 X1 Y1]
 *     DIR holds tomb-raider-link.pak (and gets a scratch .cfg); each RAM file is shown for a few frames in turn, as if played.
 * The picture shows game graphics: keep OUT outside the repo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../linkscr.h"
static uint8_t RAM[0x200000]; static uint16_t px[LINK_W * LINK_H];
static int loadRam(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return 0; int ok = fread(RAM, 1, sizeof RAM, f) == sizeof RAM; fclose(f); return ok; }
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "linkhost DIR OUT.ppm RAM.bin ... [tap x y] [drag x0 y0 x1 y1]\n"); return 1; }
    link_init(argv[1]);
    LinkIn in; memset(&in, 0, sizeof in); in.ram = RAM; in.fps = 60; in.drawn = 60; in.n3ds = 1; in.dynarec = 1; in.nowMs = 10000;
    static char s0[] = "saved", s1[] = "empty"; in.state[0] = s0; in.state[1] = s1;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "tap") && i + 2 < argc) {
            int x = atoi(argv[i + 1]), y = atoi(argv[i + 2]); i += 2;
            link_touch(TOUCH_DOWN, x, y, &in); link_touch(TOUCH_UP, x, y, &in);
        } else if (!strcmp(argv[i], "drag") && i + 4 < argc) {
            int x0 = atoi(argv[i + 1]), y0 = atoi(argv[i + 2]), x1 = atoi(argv[i + 3]), y1 = atoi(argv[i + 4]); i += 4;
            link_touch(TOUCH_DOWN, x0, y0, &in); link_touch(TOUCH_HELD, (x0 + x1) / 2, (y0 + y1) / 2, &in); link_touch(TOUCH_HELD, x1, y1, &in); link_touch(TOUCH_UP, x1, y1, &in);
        } else if (!loadRam(argv[i])) { fprintf(stderr, "ram? %s\n", argv[i]); return 1; }
        for (int k = 0; k < 3; k++) { in.nowMs += 100; link_draw(px, &in); }
    }
    in.nowMs += 300; link_draw(px, &in);
    if (getenv("BENCH")) {   /* BENCH=n: time n redraws (the PC is far quicker than a 3DS: compare builds, not machines) */
        int n = atoi(getenv("BENCH")); struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) { in.nowMs += 100; link_draw(px, &in); }
        clock_gettime(CLOCK_MONOTONIC, &b); printf("bench: %.3f ms a draw\n", ((b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6) / n);
    }
    char st[200]; link_status(st, sizeof st); printf("%s\n", st);
    FILE *f = fopen(argv[2], "wb"); fprintf(f, "P6 %d %d 255\n", LINK_W, LINK_H);
    for (int i = 0; i < LINK_W * LINK_H; i++) { unsigned p = px[i]; unsigned char c[3] = { (unsigned char)((p >> 11) << 3), (unsigned char)(((p >> 5) & 63) << 2), (unsigned char)((p & 31) << 3) }; fwrite(c, 1, 3, f); }
    fclose(f);
    return 0;
}

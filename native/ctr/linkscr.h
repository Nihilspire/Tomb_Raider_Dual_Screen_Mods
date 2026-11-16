/* linkscr.h - Tomb Raider Link on the 3DS's bottom screen, as main.c sees it. It reads the game's RAM directly, the way
 * app/www/index.html reads it through Dual's bridge. */
#ifndef LINKSCR_H
#define LINKSCR_H
#include <stdint.h>

#define LINK_W 320
#define LINK_H 240

typedef struct {
    const uint8_t *ram;            /* the PS1's 2 MB of main RAM */
    uint32_t nowMs;                /* a clock in ms */
    /* what the SYSTEM page shows */
    int fps, drawn;                /* game frames and pictures in the last second */
    int msEmu, msTop, msLink, msMax;   /* tenths of a ms a frame on average, and the longest loop */
    int n3ds, dynarec, frameskip, old3ds;
    const char *msg;               /* "SAVED TO SLOT 1", ... */
    const char *state[2];          /* a word about each save-state slot */
} LinkIn;

/* what a touch asks main.c to do */
enum { LINK_NONE, LINK_SAVE0, LINK_SAVE1, LINK_LOAD0, LINK_LOAD1, LINK_FRAMESKIP, LINK_OLD3DS, LINK_QUIT };
enum { TOUCH_DOWN, TOUCH_HELD, TOUCH_UP };

void link_init(const char *dir);                   /* the folder beside the disc: tomb-raider-link.pak and .cfg */
void link_draw(uint16_t *px, const LinkIn *in);    /* px: LINK_W x LINK_H, row by row */
int  link_touch(int phase, int x, int y, const LinkIn *in);
int  link_interval(void);                          /* game frames between redraws */
void link_flush(void);                             /* writes the settings and story progress if they changed */
void link_status(char *buf, int n);                /* a line for the log: level, room, step */

#endif

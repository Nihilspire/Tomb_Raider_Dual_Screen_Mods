# Tomb Raider Dual 3DS (alpha)

Tomb Raider (PlayStation, US) on the top screen of a **New 3DS or New 2DS**, and Tomb Raider Link on the bottom screen: the mini map
and the story steps, the same ones the phone and Windows apps show. It's the pcsx_rearmed core Tomb Raider Dual uses on Android, built for
the 3DS the way Xenogears Dual 3DS is.

## Putting it on the SD card

Extract the zip at the root of the SD card. That gives `3ds/tomb-raider-dual/` (the `.3dsx` for the Homebrew Launcher) and
`cias/Tomb Raider Dual 3DS.cia` (install it with FBI for a HOME Menu icon). Then put into `3ds/tomb-raider-dual/`:

- **your Tomb Raider disc image**: the `.cue` with its `.bin` files, or a `.chd` or `.pbp`. US releases: v1.0, v1.1 and the Greatest Hits
  discs (Rev 5, Rev 6; pick Tomb Raider in their demo menu). No game data ships with this.
- **`tomb-raider-link.pak`** (about 42 MB): the map pictures, the game's names and its lettering, made from your own disc (see below).
  Without it the game still runs and the story steps still show, in the system font, with no map.
- optional: a real BIOS (`scph*.bin`). Otherwise the core's built-in one is used.

It needs **Luma3DS** (for the recompiler; without it the core drops to its interpreter, far too slow). Sound needs the usual DSP firmware dump
at `sdmc:/3ds/dspfirm.cdc`, or it plays silent.

## Buttons

| 3DS | PlayStation |
|---|---|
| A / B / X / Y | Circle / Cross / Triangle / Square |
| L / R / ZL / ZR | L1 / R1 / L2 / R2 |
| D-pad or Circle Pad | D-pad |
| Start / Select | Start / Select |

## The bottom screen

- **The map** turns and zooms with the buttons on its right, and drags with a finger; it goes back to Lara after 15 seconds, or at once with
  the ring button. Rooms you haven't been in stay hidden (Reveal as you explore), the way to the next step is the green dashed line, and the
  step's spot pulses (an arrow at the edge when it's off the map). Pickups show with their own pictures, faded once taken.
- **The step** under the map: the dots are the level's steps (bright: the one to do, filled: done). The arrows at the sides look at the
  steps before and after; tap the row of dots to fold the panel to one line. "Carrying" lists the keys and puzzle items Lara holds.
- **The star counter** in the corner opens the level's secrets: how far and which way, or found.
- **The gear** opens SYSTEM: two save-state slots, frame skip, old-3DS speed, quit, the timings, and the map's settings.
- **Less hand-holding** (0.2-alpha, in SYSTEM): **Show: COMPASS** swaps the map for a compass turned like the camera, its needle pointing to
  the next corner of the way; **Hint text: OFF** shows only "Step 3 of 11". The way line follows the walkable floors from the nearest one
  when Lara is on a ledge or in water, says "high climbs" when it needs climbs higher than Lara can pull up, and is dotted ("no walkable
  way") when there is none.

Settings and story progress are kept in `tomb-raider-link.cfg`, the memory card in `tombraider.srm`, the states in `tombraider.st1`/`.st2`,
all beside the disc. `tomb-raider-3ds.log` there has the start-up messages and, every 5 seconds, the frame rate and how long each part of a
frame took (the Link screen's draws too): that file is what tells real speed.

Not in this alpha: Dual's analog controls and detached camera, the dev codes, the 3D map, the ending card and Record route.

## Making tomb-raider-link.pak

From the project folder, in WSL (node, the app's own level readers):

```
node native/ctr/tools/mk_pak.js "<your Track 01 .bin, .cue or .chd>" tomb-raider-link.pak
```

It holds game graphics from your disc, so it stays on your SD card: it's not in the `.3dsx`, the `.cia` or the zip.

## Build (WSL, devkitPro at /opt/devkitpro, node)

```
make -C native/ctr                # -> ~/tr3ds/ctr/Tomb Raider Dual 3DS.3dsx
native/ctr/cia/build_cia.sh       # -> ~/tr3ds/ctr/Tomb Raider Dual 3DS.cia
```

The core's source is Xenogears Dual's `third_party/pcsx_rearmed` (c881679, unmodified; `XGDUAL=` to point elsewhere), compiled with
Xenogears Dual 3DS's flags (ari64 recompiler, Unai software renderer, sound on core 1, drawing on core 2). `main.c` is Xenogears Dual 3DS's
frontend with Tomb Raider's disc, pad and touch. `linkscr.c` is `app/www/index.html`'s map and steps in C, rule for rule (what counts as
taken, when a step is done, the route over the walkable boxes, the secrets grouping). `tools/mk_hints.js` turns `app/www/hints.js` into C at
build time, so the steps keep one source; `tools/mk_pak.js` reads the disc with `trlevel.js`, `trmap.js` and `trbuild.js`.

## Tests

```
native/ctr/test/linkhost.sh OUT.png "Hint states/states/L2_S4.bin" [tap X Y] ...   # the bottom screen on the PC from a RAM dump
native/ctr/test/build_test.sh native/ctr/test/ts_state.h                          # a build that plays a script
native/ctr/test/azahar_run.sh ~/tr3ds/ctr-test/"Tomb Raider Dual 3DS.3dsx" OUT "Hint states/states/L2_S4.state"
```

`linkhost.sh` with `BENCH=300` times the drawing. The test builds load a step state (the Dual driver's `.state` files load: same core),
write both screens as files at chosen frames (`TEST_SHOT`) and quit by themselves; `azahar_run.sh` (display :91) turns them into PNGs.

## Things to know

- **Azahar can't judge speed** (it counts instructions). In it the Link screen's draw is about 9 ms, 6 to 10 times a second; a draw due on a
  frame that has already used 60% of its time waits for a quieter frame (half a second at most). Read the log from a real New 3DS before
  changing any of that.
- **One cold boot in Azahar stopped** during the core's start-up (the log ended at its `vram:` line) and the next one ran. Not seen since; if
  a device log ends there, that's where to look.
- From Xenogears Dual 3DS: `cfguInit()` first or the core skips its drawing thread; `__ctr_svchax` is the frontend's to set; Azahar has no
  svc 0x80, so the hardware cache path hasn't been run in an emulator. The `.rsf` keeps the DSP memory mapping (a CIA's data abort on
  hardware without it).
- Nothing counts as working until it's been seen on the 3DS itself.

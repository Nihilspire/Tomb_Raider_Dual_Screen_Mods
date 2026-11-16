# Tomb Raider Dual

Tomb Raider (PlayStation) on two screens: the game on one, a **Tomb Raider Link** companion screen on the other. It
shows a live mini map of the rooms around Lara, drawn from the level's own geometry and textures (or the whole level in
3D), story steps through every level with the way there drawn on the map (Waypoint system), the keys and puzzle items she carries,
pickups and secrets, and a signed record of your run at the end. For less hand-holding, the map can become a compass
that only points the way, and the hint text can be switched off. It runs as an Android app (dual-screen handhelds,
Surface Duo, a second display, or one screen with a slide-out Link panel) and on the New 3DS and New 2DS. There's also
**Tomb Raider Link**, the companion screen on its own (Windows and Android), for playing in RetroArch if you'd rather
keep your own setup, for example for hardcore RetroAchievements.

**No game files are included.** You supply your own *Tomb Raider* disc image (USA, SLUS-00152: v1.0, v1.1, or the
Greatest Hits discs Rev 5 and Rev 6) as a `.cue` with its `.bin`, a `.chd` or a `.pbp`. The room pictures, the 3D
level data, the font, the item pictures, level and item names, and the title and loading screens used as the theme are
all built from it on first start, and nothing from it ever leaves your device.

## Screenshots
<img width="960" height="432" alt="07 Link beside the game (one screen) - Tomb of Qualopec" src="https://github.com/user-attachments/assets/297a7067-1aff-4f46-a6b6-0018ac7e8693" />
<img width="656" height="872" alt="06 Settings, hint text off, compass on - Palace Midas" src="https://github.com/user-attachments/assets/09f70672-90bd-46ed-b33a-6c743ae63521" />
<img width="656" height="872" alt="04 Browse the story steps - St  Francis&#39; Folly" src="https://github.com/user-attachments/assets/54dba303-6ef4-4726-b969-02da19e51096" />
<img width="656" height="872" alt="03 Compass for less hand-holding - City of Khamoon" src="https://github.com/user-attachments/assets/4f6a075d-714e-44ab-96f9-47d28b14d698" />
<img width="656" height="872" alt="02 The 3D map - City of Vilcabamba" src="https://github.com/user-attachments/assets/b7a87dd8-3e38-4c23-95f6-ceefedd78a46" />
<img width="656" height="872" alt="01 Story steps and guide lines - Caves" src="https://github.com/user-attachments/assets/a2650b53-eb1a-44a2-a804-dc8554616993" />



## AI-assisted?
Feel free to skip over if you feel strongly about it. Claude's `Opus 5.5` was used for ram mapping and coding with help form `Qwen-3-Coding` for code review. Code is
open-source, feel free to use as a base and human it up.

## Downloads
See [Releases](../../releases). What changed in the 3DS build: [native/ctr/README.md](native/ctr/README.md).

| File | What it is |
|---|---|
| `Tomb.Raider.Dual.<ver>.apk` | Android: emulator + Link screen in one app |
| `Tomb.Raider.Link.<ver>.apk` | Android: Link screen only, reading Tomb Raider running in RetroArch (pcsx_rearmed core) over Network Commands (port 55355) |
| `Tomb.Raider.Dual.3DS.<ver>.SD.root.zip` | New 3DS / New 2DS: extract at the SD card's root. Holds the `.cia` (install with FBI) and the `.3dsx` (Homebrew Launcher). Needs Luma3DS. Will not work on old 3ds; it needs the newer processor |
| `Tomb.Raider.Map.Viewer.html` | Any browser: open your disc and see any level in 3D, with the way to any item. An offline tool; can be used on any html capable device. Requires a disc image to populate the images |

## Features
- **Mini map**: the room around Lara, drawn top-down from the level's own floors and textures, following her as she
  moves. Tall rooms are cut at her height, rooms below her are faint, and the room is drawn in the current state it is in (e.g. a switch that drains water will show the room depending on the current state.) Turn it with the camera or with Lara, zoom, drag, and it drifts back to her.
- **3D map**: the whole level in 3D from your disc, with the same zoom and pan, and a YOU ARE HERE pin. Usefully for matching geometry to see where you are better or paths to what you need.
- **Reveal as you explore**: only the rooms Lara has been in, plus the way ahead.
- **Story steps** for every level from Lara's Home to the Great Pyramid, with the item's picture. A step ticks off
  when its key is taken, its switch pulled or its slot filled, in any order. Browse the steps ahead with the arrows. Basically a modern day waypoint system.
- **The way to the next step**: a green line over the floors Lara can walk, with ▲/▼ for climbs and drops and how far
  it is ("41 blocks N · 2 up · 6 rooms"). Hand-drawn guide lines where they exist.
- **Less hand-holding**: a **compass** in place of the map, whose needle points to the next objective; **hint
  text** off (just "Step 3 of 11"); the next step off entirely.
- **Pickups and secrets**: keys, puzzle items, guns, ammo and medi packs with their own pictures (taken ones fade);
  secrets as stars, a counter, and a list with how far each one is.
- **Ending card**: when the Great Pyramid ends, every level's time, kills, pickups and secrets, and the total. Sign it
  by touch and save it; signed runs are kept in Settings. Includes the current tiem and date taken from the device for timestamp purposes.

- **Tomb Raider Dual only**:
  - **Screens**: Link on a second display or the other half of a Surface Duo, the screens swapped, or on one screen
    either as a **slide-out** (a tab on the game's edge slides Link in over it) or beside the game.
  - The game's window framed by the level's own floor texture, or your own bezel.
  - **Analog controls** (Mods): Lara turns to face the stick's direction and the camera stays where you put it; Toggled OFF by default.
    **detached camera** on the right stick. The game itself isn't patched. Somewhat buggy. Toggled OFF by defualt.
  - Save states with pictures, controller remapping, hotkeys (quick save, quick load, fast forward, the Link panel).
  
- **Tomb Raider Dual 3DS** (alpha): the game on the top screen; the map, compass, story steps, secrets and save states
  on the bottom.


## Credits and licences
This project is licensed under the **GNU General Public License v2.0 or later**.
If you do use parts of it in your project, please credit me as a "playtester".

The release builds include:
- **PCSX-ReARMed** (GPL-2.0), github.com/libretro/pcsx_rearmed, commit `c881679`, unmodified as a library (Dual APK),
  and built from source for the 3DS with its own frontend.
- **libctru** (zlib), github.com/devkitPro, unmodified, and **newlib** (BSD-style licences), the C library as built by
  devkitARM (3DS).

The level file reader follows OpenLara's `loadTR1_PSX` (github.com/XProger/OpenLara, BSD-2-Clause). RAM addresses were
found by reading the running game live in RetroArch, by poking it in a headless copy of Dual's emulator, and from
community save states matched against the level files.

Tomb Raider and Lara Croft are trademarks of their respective owners; Tomb Raider (PlayStation) © 1996 Core Design. This
is an unofficial fan project and is not affiliated with Core Design, Eidos, Square Enix, Crystal Dynamics or Embracer
Group.

## RAM / disc data used (Tomb Raider USA, Rev 6)
Everything the apps read, and how each address was found: [docs/RAM_map.md](docs/RAM_map.md).

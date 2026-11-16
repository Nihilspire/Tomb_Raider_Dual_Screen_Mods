# Tomb Raider Link and Tomb Raider Dual: features

_Link 0.14.0, Dual 0.16.0, Dual 3DS 0.1-alpha (15 Nov 2026). For Tomb Raider on the PlayStation, US release (v1.0, v1.1, Greatest Hits Rev 5 and Rev 6)._

## The apps
- **Tomb Raider Link** (Windows and Android): a second screen beside RetroArch. It reads the game's memory over RetroArch's network commands, and only reads.
- **Tomb Raider Dual** (Android): the game and Link in one app, for dual-screen devices (Retroid Pocket Duo, AYN Thor, Surface Duo, a phone with a second display) or one screen.
- **Tomb Raider Dual 3DS** (New 3DS, alpha): the game on the top screen, Link's map and steps on the bottom.
- **Map viewer**: one HTML file for a browser that draws any level in 3D from your disc and the way to any item.
- Everything is built from **your own disc** the first time it starts. No game data ships with the apps.

## The map
- **Mini map** of the room Lara is in and the rooms around her, made from the level's own floor textures and drawn top-down. It follows her as she moves.
- **Tall rooms** are cut at Lara's height, so floors above her don't cover her. Rooms below are drawn faintly.
- **Turn the map** with the camera or with Lara, or keep north up.
- **Reveal as you explore**: only rooms Lara has been in, plus the way ahead.
- **3D map**: the level drawn in 3D from the disc, with the same zoom and pan. Two fingers or the right mouse button turn and tilt it. A "YOU ARE HERE" pin marks Lara.
- **Pickups**: keys, puzzle items, guns, ammo and medi packs with their own pictures. Taken ones fade.
- **Secrets**: a star on each secret (off by default), a counter in the header, and a list with how far each one is.
- **Flipped rooms** (water drained, rooms changed by a switch) are drawn as they are now.
- Pinch or the wheel to zoom, drag to look around, double-tap to go back to Lara.

## Story steps (hints)
- **Steps through every level**, from Lara's Home to the Great Pyramid, in walkthrough order, with the item's picture.
- **A step ticks off** when its item is taken, its switch pulled or its keyhole used, in any order. A place step ticks off when Lara gets there.
- **The way to the next step**: a green line over the floors Lara can walk, with ▲/▼ for climbs and drops. It says how far ("41 blocks N · 2 up · 6 rooms"). Hand-drawn guide lines are used where they exist (levels 1 to 11).
- **Browse** the steps ahead or behind with the arrows or a swipe. Fold the panel down to one line.
- **Carrying**: the keys and puzzle items Lara has.
- **Level complete** when a level ends; demos and cut scenes are recognised.

## Less hand-holding
- **Compass** instead of the map: a compass turned like the camera, with a needle to the next corner of the way. No rooms are shown.
- **Hint text off**: the panel shows only "Step 3 of 11".
- **Next step off**: no line or spot. The steps still count.
- **Secrets off** by default, and directions in the secrets list can be turned off.

## The ending
- **Ending card** when the Great Pyramid ends: every level's time, kills, pickups and secrets, and the total.
- **Sign it by touch**, save it, and see signed runs later in Settings.
- **DEV CODES USED** on the card if any dev code was used during the run (Dual).
- Laid out for short landscape screens such as the Retroid Pocket Duo, AYN Thor and Surface Duo bottom screens, with the signature box always in view.

## Settings
- Grouped as **Map**, **Hints**, **On the map**, **Quality of life** (smooth movement, map returns to Lara, fading map buttons) and **Story**.
- **Developer**: the memory read mode (Link), rebuild the map files from the disc, Record route.
- Text everywhere is in **the game's own lettering**, made from the font on your disc.

## Tomb Raider Dual only
- The game runs in the app (PCSX ReARMed), with save states, controller support and a launcher.
- **Screens**: Link on the second screen (or the other half of a Surface Duo), the two screens swapped, or on one screen either **slide-out** (the game fills the screen and a tab on its right edge slides Link in over it; Back or a controller hotkey puts it away) or **beside the game** (the game 4:3 on one side, Link on the other).
- **Bezel**: the game in a window on a dark backdrop, or your own bezel picture.
- **Analog controls** (Mods): Lara turns to the stick's direction and the camera stays where you put it. The game itself isn't patched.
- **Detached camera** (Mods): the right stick turns the camera and nothing else does.
- **Dev codes** (enter the Konami code on a menu): invincible, infinite air, infinite ammo, full health, all weapons, medi packs, this level's keys, warp to any level, next level, restart the level, and **warp to the ending and credits**. The warp answers the memory card question with "Exit without saving", so your save is never overwritten.

## For making steps (Windows)
- **Story step editor**: every level in 3D from the disc. Place a step's spot, link its items, draw its guide line, and save it to Link.
- **Record route**: play a level and Link writes its steps and guide lines as you go.

## Not yet seen on a real device
Every build so far has been tested headless and by screenshot. Every story step still starts with "?" until it has been checked in the live game.

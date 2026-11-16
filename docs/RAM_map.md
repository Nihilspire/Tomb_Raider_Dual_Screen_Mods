# Tomb Raider (PlayStation, US, SLUS-00152 Rev 6): RAM map

Addresses are PS1 main RAM offsets (add 0x80000000 for the CPU address). Found on 2 Nov 2026 from:
- live reads from RetroArch: pcsx_rearmed, `READ_CORE_MEMORY` at offset 0 (mode `m00`), US disc, Jack's DexDrive save (finished
  game, level select) and two attract demos;
- 153 ePSXe save states (PAL, SLES-00024) whose notes say what happened in each, matched against the level files.

**PAL → US:** the game's variables sit exactly 0x90 higher in the PAL build. The level number (0x876F8 → 0x87668), item
pointer (0x89C8C → 0x89BFC), flip state and progress block all line up, and a byte-by-byte comparison of the memory round
each address agrees (`tools/saves/align.js`). "Seen in US" below means read live from the US game; "PAL" means found in the
PAL saves and moved by −0x90.

**US v1.0 (first release) → Rev 6:** on v1.0, SLUS_001.52 is the game itself (Rev 6's SLUS_001.52 is the Greatest Hits demo
menu, which starts TOMBRAID.EXE). Its variables below 0x89000 sit 0xE8 lower (level 0x87580, play/demo 0x884D2, key ring
0x886D8) and those from 0x89000 up 0x100 lower (items 0x89AFC, Lara 0x89EAC, flip 0x89C54, camera 0x89B34, progress 0x927F8,
secrets 0x92800). Found by lining up the RAM of both at the start of Caves (`tools/test/rev_probe.py`, `rev_shift.py`). The
page tells them apart by the level name table: 0x87710 (Rev 6) or 0x87628 (v1.0) holds pointers to "Lara's Home" and "Caves",
8 bytes apart. **Rev 5** (Greatest Hits with the Gex 3 / Soul Reaver / TR3 demos) has the same TOMBRAID.EXE as Rev 6, byte
for byte, and the same level files; its RAM at the start of Caves lines up with Rev 6 at every address. **v1.1** (no demos, like v1.0) boots SLUS_001.52 as the
game, and that file is byte for byte Rev 6's TOMBRAID.EXE: same level files (Natla's Mines has Rev 6's box fix), same RAM. Read in v1.0 so far: level, Lara, items (headless boot into Caves); the rest only by the byte alignment.

## Globals

| Address | Size | What | Seen in |
|---|---|---|---|
| 0x87670 | u8 | 1 from the moment the level ends (end trigger reached) through the stats screen; 0 again in the next level | US headless: Lara put on Caves' exit square (tools/test/endflag.py) |
| 0x87668 | s16 | Current level: 0 Lara's Home, 1 Caves … 15 The Great Pyramid, 16–19 cut scenes, 20 title. Changes before the new level's items are in place, so check Lara is inside her room | US |
| 0x885BA | s16 | −1 while a game is being played; 1 at the title and all through a demo (loading included) | US (2 demos, title, 5 real) + PAL (153) |
| 0x89BFC | u32 | Pointer to the item array | US |
| 0x89FAC | u32 | Pointer to Lara's item | US |
| 0x89C34 | 3×s32 | Camera position x, y, z; room s16 at 0x89C40 | US |
| 0x89C44 | 3×s32 | The point the camera looks at (Lara's head); room s16 at 0x89C50. Camera heading = atan2(Δx, Δz) | US |
| 0x89C54 | s32 | Camera type: 0 is the one that follows Lara. Layout after the PC game's CAMERA_INFO | US headless |
| 0x89C6C | s32 | Camera distance (0x600) | US headless |
| 0x89C74, 0x89C78 | s16, s16 | Camera aim, added to Lara's heading and tilt; the game clears them each frame, so a write between frames lasts one frame. Written by Dual's analog controls. 0x89C76 is the camera's actual angle | US headless (tools/test/analog_test.py) |
| 0x89D54 | u8 | 1 while the alternate (flipped) rooms are in. Set only in the 12 levels that have alternate rooms; the Sanctuary drain and the Cistern's water turn it on | PAL + US live: Qualopec's first room, 1 on the flip-on sector, 0 on the flip-off sector, back to 1, as Lara walked over them |
| 0x89E46–0x89E68 seen | u16[] | Flip switches: one word per flip number, 0x3E00 (or 0x3F00 with the once bit) when its switches are all used. Where the array starts is not known | PAL |
| 0x928F8 | u32 | Level timer (frames) | US (ticks live) + PAL |
| 0x928FC | u16 | Kills | PAL |
| 0x92900 | u8 | Secrets found this level, one bit per secret number (floor-data trigger action 10's parameter) | PAL + US live: 0 → 0x02 when Lara stood on Vilcabamba's secret-1 trigger sector |
| 0x9290E | u16 | Pickups this level | PAL + US live (0 → 1 when Jack took the Silver Key) |
| 0x92912–0x92919 | u8 ×8 | Puzzle items 1–4, keys 1–4: counts that follow the PAL saves but do **not** change live when a key is picked up (US, Silver Key stayed 0), so they are only written at some later point (saving?). Not used | PAL |
| 0x887C0 | u16 | Items in the inventory's key ring (0 → 1 when the Silver Key was picked up, 1 → 0 when it went in its lock) | US live |
| 0x887C8 | u16[] | Quantity of each key-ring item (slot 0: 1 → 0 when the key was used) | US live |
| 0x8E66C | u8 | Another "held" counter for that key: 0 → 1 on pickup, 1 → 0 on use (probably one byte per inventory object; not mapped further) | US live |
| 0x887F8 | u32[] | Key-ring items: pointers to the game's inventory-item table (RAM 0x80087FAC + 0x40·n). The entry's u16 at +4 is the inventory object number: 114–117 puzzle items 1–4, 127 lead bar, 133–136 keys 1–4, 150 scion. Slots past the count keep old pointers | US live |
| 0x88854, 0x88858 | u32, u32[] | The options ring: count, then the same kind of pointers | US |
| 0x92902 | u8 | Not the level: 15 after loading the DexDrive save, 0 on a new game (the loaded save's level?) | US |

Not demo flags, though they looked like it from a few dumps: 0x875FC (stays 1 after a demo has run), 0x89A85 (1 in most
mid-level play).

When the rooms are flipped, the game swaps the rooms' contents and keeps the numbers: Lara's room stays the normal room's
number (Cistern saves: rooms 56 and 49 while flipped). Draw room *i* with its alternate's picture while 0x89D54 is 1.

The item array is on the heap: its address changes from level to level (Caves 0x176814, Vilcabamba 0x17F9BC). Always go
through the pointers. An old level's array can stay in RAM after a level change.

## Item record (72 bytes, the same layout as the PC game's ITEM_INFO)
| Offset | Size | What |
|---|---|---|
| +0x00 | s32 | floor height under the item |
| +0x0C | u16 | type (object number). Puzzle slots change type when filled (118–121 → 122–125); a gun Lara already has turns into its ammo (85–87 → 89–91) |
| +0x0E | s16 | current animation state |
| +0x18 | s16 | room |
| +0x1A | s16 | next item in the room (−1 = none) |
| +0x22 | s16 | health (Lara: 1000) |
| +0x28 | u16 | flags: bits 9–13 (0x3E00) are the trigger mask; a switch (types 55, 56) has all five set once pulled |
| +0x2C | u32 | data pointer (Lara: her LARA_INFO) |
| +0x30, +0x34, +0x38 | s32 | x, y, z (y points down; 1024 = one block) |
| +0x3C, +0x3E, +0x40 | s16 | rotation x, y, z (y: 0 = north (+z), 0x4000 = east (+x)) |
| +0x44 | u16 | status bits: bit 0 active, bits 1–2 status (0 none, 1 active, 2 done, 3 invisible), bit 3 gravity |

A pickup that has been taken has status 3 (`(w >> 1) & 3 == 3`): pickups go 0x20 → 0x26 when taken, 0x20 → 0x24 when used.
A keyhole (types 137–140) goes 0x20 → 0x24 (status 2) when its key is used: seen live (Vilcabamba's Silver Key lock, item 9)
and in the PAL saves.
Taking a pickup also sets its y rotation to Lara's. +0x42 is not the status word on the PlayStation build.

## Live test notes (2 Nov 2026)
- `tools/ram/poke.js` writes RAM through RetroArch (`WRITE_CORE_MEMORY`); `tools/ram/watch.js` prints Lara and chosen bytes.
  Moving Lara is three writes to her item: room (+0x18), x (+0x30), z (+0x38).
- In Jack's RetroArch, Enter is both Cross and Start, and Escape is both Circle and Quit, so pickups were done by Jack on a
  controller. `tools/ram/afterpick.js` waits for an item to be taken and lists the bytes that went up by 1 against a "before"
  dump (RetroArch's `PAUSE_TOGGLE` command freezes the game while the "before" dump is taken).
- The level number changes before Lara's pointer does: 10 s after Vilcabamba loaded, 0x89FAC still pointed at Qualopec's Lara.

## The end of the game (6 Nov 2026, US headless, Lara on the Great Pyramid's real exit square; `tools/test/credits_watch.py`, `credits_full.py`, `credits_check.py`)
There is no credits flag as such: a search of all 2 MB found nothing else but the stack that holds one value through the credits and no other. What marks them is the screen buffer pointer at 0x8742C (the same at 0x87430), read together with the level and ending flag.

| Stage | Level 0x87668 | Ending 0x87670 | Pointer 0x8742C |
|---|---|---|---|
| Play, stats, save prompt | 15 | 0 then 1 | 0x80093544 |
| The film of the pyramid | 15 | 1 | 0x8008C620 |
| THE END (DELDATA/END.RAW), Created By (CRED.RAW) | 15 | 1 | **0x807FF5A8**, 0x800DD564 for a moment between the two |
| Title | 20 | 0 | 0x8008C620, then 0x80093544 |

So: credits = level 15, ending 1, pointer from 0x807F0000 up. Other still pictures use that range too (0x807FF568, 0x807FF570 while levels load), which is why level and ending are needed, and it's why a range is safer than the exact value. Seen in two runs with the same 0x807FF5A8, both from a save state, not a played game. v1.0's address would be 0x87344 by the 0xE8 shift; not tested.

## Inventory and Lara's data (6 Nov 2026, US headless, `tools/test/dev_test.py`)
| Address | Size | What |
|---|---|---|
| 0x8872C, 0x88734, 0x88764 | s16, s16[], u32[] | Main ring: count, quantities, pointers to the inventory-item table. Keys ring is the same at 0x887C0, 0x887C8, 0x887F8 |
| 0x87FAC + 0x40·n | 64 bytes | Inventory-item table. +4 object number, +0x38 ring order. n: 0 compass, 1 pistols, 2 shotgun, 3 magnums, 4 uzis, 5-8 their ammo, 9 explosive, 10 medi pack, 11 large medi pack, 16-19 puzzle items, 20-23 keys. An item whose model isn't in the level is invisible and has no name |
| LARA_INFO (item +0x2C) | | +0x0A water status (3, the PC game's fly mode, only freezes her here), +0x12 s16 air (1800), s32 ammo: +0xA4 magnums, +0xB0 uzis, +0xBC shotgun (pistols never run out). Ammo for a gun she doesn't have is an item in the ring |
| Lara's item +0x20 | s16 | Fall speed |

Level warp: write 1 to the ending flag 0x87670 and the number before the wanted level to 0x87668, and the game goes on to the wanted one. A cut scene's number (16-19) goes to the level after it without playing it: 5, 10, 14, 15. Leaving Lara's Home also needs 0x885BA = -1, or the next level runs as a demo. Level −1 hangs, so this can't reach Lara's Home.

## Still unknown
- Items dropped by enemies (Pierre's, Larson's) are not in the level's item list; the held counts above cover their keys.

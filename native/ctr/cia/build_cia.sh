#!/bin/bash
# build_cia.sh: Tomb Raider Dual 3DS as a .cia (HOME Menu title), from the .3dsx build's ELF (make first). Needs makerom and
# bannertool in $TOOLS (default /home/claude/work/ctr/tools) and Pillow. The banner is the project's own plain lettering (no game
# logo or font, no mascot) and silent.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
TOOLS=${TOOLS:-/home/claude/work/ctr/tools}
OUT=${OUT:-$HOME/tr3ds/ctr}
VER=$(grep -E '^VERSION' "$HERE/Makefile" | sed 's/.*:= *//')
B=$OUT/cia; mkdir -p "$B"
python3 - "$B/banner.png" <<'EOF'
import sys
from PIL import Image, ImageDraw, ImageFont
im = Image.new('RGBA', (256, 128), (12, 10, 8, 255)); d = ImageDraw.Draw(im)
d.rectangle((0, 96, 255, 127), fill=(40, 30, 16, 255)); d.line((0, 96, 255, 96), fill=(168, 128, 58, 255), width=2)
f = ImageFont.load_default(size=30)
def thick(xy, s, font, fill):   # thick lettering with a dark 3D edge
    for o in range(4, 0, -1): d.text((xy[0] + o, xy[1] + o), s, font=font, fill=(58, 20, 6, 255), anchor='mm')
    d.text(xy, s, font=font, fill=fill, anchor='mm')
thick((128, 32), 'TOMB RAIDER', f, (226, 191, 106, 255))
thick((128, 66), 'DUAL', f, (239, 230, 210, 255))
d.text((128, 112), 'Map and story steps below', font=ImageFont.load_default(size=13), fill=(239, 230, 210, 255), anchor='mm')
im.save(sys.argv[1])
EOF
python3 -c "import wave,sys; w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728); w.writeframes(bytes(4*3273)); w.close()" "$B/silence.wav"
"$TOOLS/bannertool-bin" makebanner -i "$B/banner.png" -a "$B/silence.wav" -o "$B/banner.bnr"
"$TOOLS/bannertool-bin" makesmdh -s "Tomb Raider Dual 3DS" -l "Tomb Raider Dual 3DS $VER: your disc + a Link screen" \
    -p "Jack" -i "$HERE/tools/icon.png" -o "$B/icon.icn"
"$TOOLS/makerom-bin" -f cia -o "$OUT/Tomb Raider Dual 3DS.cia" -target t -exefslogo \
    -elf "$OUT/tombraider3ds.elf" -rsf "$HERE/cia/tombraiderdual.rsf" -icon "$B/icon.icn" -banner "$B/banner.bnr" \
    -major "$(echo $VER | cut -d. -f1)" -minor "$(echo $VER | cut -d. -f2 | cut -d- -f1)" -micro 0
ls -la "$OUT/Tomb Raider Dual 3DS.cia"

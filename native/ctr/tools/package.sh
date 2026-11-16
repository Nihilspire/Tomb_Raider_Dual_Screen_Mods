#!/bin/bash
# package.sh: the release zip, extracted at the SD card's root: 3ds/tomb-raider-dual/ (the .3dsx, README, the core's GPL licence)
# and cias/ (the .cia). Build both first (make, cia/build_cia.sh). The pack is game data from the player's disc: never in the zip.
#   native/ctr/tools/package.sh <out dir>
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-$HOME/tr3ds/ctr}
VER=$(grep -E '^VERSION' "$HERE/Makefile" | sed 's/.*:= *//')
XGDUAL=${XGDUAL:-$(cd "$HERE/../../../Xenogears/xenogears-dual" && pwd)}
D=$(mktemp -d); mkdir -p "$D/3ds/tomb-raider-dual" "$D/cias"
cp "$OUT/Tomb Raider Dual 3DS.3dsx" "$HERE/README.md" "$D/3ds/tomb-raider-dual/"
cp "$XGDUAL/third_party/pcsx_rearmed/COPYING" "$D/3ds/tomb-raider-dual/pcsx_rearmed-GPL-LICENSE.txt"
cp "$OUT/Tomb Raider Dual 3DS.cia" "$D/cias/"
Z="$1/Tomb Raider Dual 3DS $VER (SD root).zip"; rm -f "$Z"
(cd "$D" && python3 -c "
import zipfile, os, sys
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as z:
    for top in ('cias', '3ds'):
        for r, _, fs in os.walk(top):
            for f in sorted(fs): z.write(os.path.join(r, f))
" "$Z")
rm -rf "$D"
python3 -c "import zipfile,sys; [print(' ', i.filename, i.file_size) for i in zipfile.ZipFile(sys.argv[1]).infolist()]" "$Z"

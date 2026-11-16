#!/bin/bash
# linkhost.sh OUT.png [RAM.bin ...] [tap X Y] ...: builds test/linkhost.c (the 3DS Link screen on the PC) and draws it from RAM dumps.
# Needs ~/tr3ds/tomb-raider-link.pak (tools/mk_pak.js); a scratch copy of the settings lives in ~/tr3ds/host.
# PAK_DIR overrides where the pack is. The output shows game graphics: keep it outside the repo.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
B=${B:-$HOME/tr3ds/host}; mkdir -p "$B"
PAK_DIR=${PAK_DIR:-$HOME/tr3ds}
[ -e "$B/tomb-raider-link.pak" ] || ln -sf "$PAK_DIR/tomb-raider-link.pak" "$B/tomb-raider-link.pak"
if [ "$HERE/tools/mk_hints.js" -nt "$B/hintdata.h" ] || [ "$ROOT/app/www/hints.js" -nt "$B/hintdata.h" ]; then node "$HERE/tools/mk_hints.js" "$ROOT/app/www/hints.js" "$B/hintdata.h" >/dev/null; fi
gcc -O1 -g -Wall -Wno-unused-function -Wno-misleading-indentation -I"$B" -I"$HERE" "$HERE/test/linkhost.c" "$HERE/linkscr.c" -lm -o "$B/linkhost"
OUT=$1; shift
[ -n "$FRESH" ] && rm -f "$B/tomb-raider-link.cfg"
"$B/linkhost" "$B" "$B/out.ppm" "$@"
python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" "$B/out.ppm" "$OUT"

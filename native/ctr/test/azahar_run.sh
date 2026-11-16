#!/bin/bash
# azahar_run.sh APP OUTDIR [STATE] [SECONDS]: boots a build (best a test build from build_test.sh, which quits by itself and writes its
# own screenshots) in Azahar headless under Xvfb (display :91, Tomb Raider's own; other projects use others) with Mesa's software
# OpenGL, as a New 3DS, with its own config and SD card in $AZHOME. The disc: ~/trdisc (tr.cue and its tracks, links to Jack's
# Rev 6 backup), the pack: ~/tr3ds/tomb-raider-link.pak; STATE is copied to the SD card as test.state (TEST_STATE in the script).
# Prints the game log and turns the test build's shot_*.raw into OUTDIR/*.png. The pictures show the game: keep OUTDIR outside the repo.
R=$(cd "$(dirname "$0")/../../.." && pwd)
APP=$(readlink -f "$1"); OUT=$2; STATE=$3; SECS=${4:-90}
AZ=${AZAHAR:-/home/claude/work/azahar/squashfs-root/usr/bin/azahar}
AZHOME=${AZHOME:-$HOME/tr3ds/azahar}
SD="$AZHOME/data/azahar-emu/sdmc/3ds/tomb-raider-dual"
mkdir -p "$OUT" "$AZHOME/config/azahar-emu" "$SD"
CFG="$AZHOME/config/azahar-emu/qt-config.ini"
if [ ! -f "$CFG" ]; then
  cp "$HOME/.config/azahar-emu/qt-config.ini" "$CFG"
  sed -i -e 's/^is_new_3ds=.*/is_new_3ds=true/' -e 's/^is_new_3ds\\default=.*/is_new_3ds\\default=false/' "$CFG"
fi
# a stand-in DSP firmware gets ndspInit through (Azahar's DSP is its own)
[ -f "$AZHOME/data/azahar-emu/sdmc/3ds/dspfirm.cdc" ] || cp "$HOME/xgtmp/azahar/data/azahar-emu/sdmc/3ds/dspfirm.cdc" "$AZHOME/data/azahar-emu/sdmc/3ds/" 2>/dev/null
for f in "$HOME"/trdisc/tr.cue "$HOME"/trdisc/t*.bin; do ln -sf "$(readlink -f "$f")" "$SD/$(basename "$f")"; done
ln -sf "$HOME/tr3ds/tomb-raider-link.pak" "$SD/tomb-raider-link.pak"
rm -f "$SD"/shot_*.raw "$SD/tomb-raider-link.cfg"
[ -n "$STATE" ] && cp "$STATE" "$SD/test.state"
export XDG_CONFIG_HOME="$AZHOME/config" XDG_DATA_HOME="$AZHOME/data"
export DISPLAY=:91 LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb
pkill -f 'Xvfb :91' 2>/dev/null; rm -f /tmp/.X91-lock
Xvfb :91 -screen 0 1280x1024x24 >/dev/null 2>&1 & XV=$!
sleep 1
"$AZ" "$APP" >"$OUT/azahar.log" 2>&1 & AP=$!
# wait for the test build to quit by itself (TEST_QUIT), at most SECS
for i in $(seq 1 "$SECS"); do sleep 1; kill -0 $AP 2>/dev/null || break; grep -q '^quit at' "$SD/tomb-raider-3ds.log" 2>/dev/null && { sleep 2; break; }; done
kill $AP 2>/dev/null; sleep 1; kill -9 $AP 2>/dev/null; kill $XV 2>/dev/null
echo "--- azahar errors"; grep -iE 'error|critical|unimpl|unknown svc|fail' "$OUT/azahar.log" | grep -viE 'libEGL|MESA|ZINK' | sort | uniq -c | sort -rn | cut -c1-200 | head -${AZLINES:-10}
echo "--- game log"; tail -${LOGLINES:-30} "$SD/tomb-raider-3ds.log" 2>/dev/null | cut -c1-220
python3 - "$SD" "$OUT" <<'EOF'
import sys, glob, os, struct
from PIL import Image
sd, out = sys.argv[1], sys.argv[2]
def rgb(p): return ((p >> 11) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3)
for f in sorted(glob.glob(sd + '/shot_*_link.raw')):
    d = open(f, 'rb').read(); px = struct.unpack('<%dH' % (len(d) // 2), d)
    im = Image.new('RGB', (320, 240)); im.putdata([rgb(p) for p in px])
    top = Image.new('RGB', (400, 240)); t = open(f.replace('_link', '_top'), 'rb').read(); tp = struct.unpack('<%dH' % (len(t) // 2), t)
    top.putdata([rgb(tp[x * 240 + 239 - y]) for y in range(240) for x in range(400)])
    both = Image.new('RGB', (400, 480)); both.paste(top, (0, 0)); both.paste(im, (40, 240))
    name = os.path.basename(f).replace('_link.raw', '.png'); both.save(os.path.join(out, name)); print('shot', name)
EOF

#!/bin/bash
# build_test.sh [script.h]: the 3DS build with a compiled-in script (an emulator's input can't be driven headless), in
# ~/tr3ds/ctr-test. The script header defines TEST_SCRIPT ({frame, frames, pad bits, tap x, tap y}, ...) and optionally
# TEST_STATE (a save state to load at frame 2: the Dual driver's .state files work, same core), TEST_SHOT (frames at which both
# screens are written beside the disc as shot_<frame>_link.raw / _top.raw) and TEST_QUIT. Without an argument: a short boot.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
T=${OUT:-$HOME/tr3ds/ctr-test}; mkdir -p "$T"
if [ -n "$1" ]; then cp "$1" "$T/test_script.h"; else
  printf '#define TEST_SCRIPT {0, 0, 0, -1, -1}\n#define TEST_SHOT 600, 1200\n#define TEST_QUIT 1300\n' > "$T/test_script.h"; fi
cd "$HERE"
# the core's objects are the same as the normal build's: reuse them
mkdir -p "$T/obj"; [ -d "$T/obj/core" ] || cp -r "$HOME/tr3ds/ctr/obj/core" "$T/obj/" 2>/dev/null || true
rm -f "$T/obj/main.o" "$T/obj/linkscr.o"
make OUT="$T" EXTRA="-DCTR_TEST -I$T" 2>&1 | grep -E 'error|Error|3dsx' | grep -v warning || true

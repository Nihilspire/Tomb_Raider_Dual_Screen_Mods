#!/bin/bash
# Builds jni/psxlink.c (Xenogears Dual's JNI frontend, JNI names under com.jack.tombraider) into out/lib/arm64-v8a/libtrlink.so.
# The core (libpcsx_rearmed.so, pcsx_rearmed @ c881679) is Xenogears Dual's prebuilt one, used unmodified.
# Uses Xenogears Dual's NDK-less Android toolchain (toolchain/android-clang + its stub sysroot): XGDUAL points at that repo.
set -e
cd "$(dirname "$(readlink -f "$0")")"
XGDUAL="${XGDUAL:-/mnt/c/Users/Jackson/Desktop/Claude/Xenogears/xenogears-dual}"
J=/usr/lib/jvm/java-17-openjdk-amd64/include
"$XGDUAL/toolchain/android-clang" -O2 -w -shared -I$J -I$J/linux -Ijni jni/psxlink.c -o out/lib/arm64-v8a/libtrlink.so \
  -Wl,-soname,libtrlink.so -Lout/lib/arm64-v8a -lpcsx_rearmed -llog -lc -ldl -Wl,-z,max-page-size=16384 -Wl,-z,defs
echo "built out/lib/arm64-v8a/libtrlink.so ($(nm -D out/lib/arm64-v8a/libtrlink.so | grep -c Java_com_jack_tombraider) JNI functions)"

#!/bin/bash
# Build and sign "Tomb Raider Link <version>.apk" without Gradle (WSL Ubuntu). Usage: TR_KS_PASS=... ./build.sh 0.3.0
# Needs: aapt, zipalign, apksigner, javac, dalvik-exchange, android-23 platform jar; keystore.jks beside this script (made on the
# first build with the password in TR_KS_PASS; keep the password yourself, it is never written to a file).
# The page is ../../app/www; nothing from the game is in the APK (the app builds its pictures from the player's disc).
set -e
cd "$(dirname "$0")"
unset JAVA_TOOL_OPTIONS
VER="$1"; [ -n "$VER" ] || { echo "usage: $0 <version, e.g. 0.3.0>"; exit 1; }
[ -n "$TR_KS_PASS" ] || { echo "set TR_KS_PASS (the keystore password)"; exit 1; }
IFS=. read -r VA VB VC <<< "$VER"; CODE=$(( 10#${VA:-0}*10000 + 10#${VB:-0}*100 + 10#${VC:-0} ))
OUT="Tomb Raider Link $VER.apk"
AJAR=/usr/lib/android-sdk/platforms/android-23/android.jar
rm -rf assets/www && mkdir -p assets && cp -r ../../app/www assets/www
sed -i "s/var VERSION='[^']*';/var VERSION='$VER';/" assets/www/index.html
rm -rf obj && mkdir obj
javac -nowarn -source 8 -target 8 -bootclasspath $AJAR -d obj src/com/dualscreen/tombraider/*.java 2>&1 | grep -v "warning\|Picked up\|^Note" || true
dalvik-exchange --dex --output=classes.dex obj && rm -rf obj
sed -i "s/versionCode=\"[0-9]*\"/versionCode=\"$CODE\"/; s/versionName=\"[^\"]*\"/versionName=\"$VER\"/" AndroidManifest.xml
if [ ! -f keystore.jks ]; then
  keytool -genkeypair -keystore keystore.jks -alias tombraider -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass:env TR_KS_PASS -keypass:env TR_KS_PASS -dname "CN=Tomb Raider Link, O=Jack" > /dev/null 2>&1
  echo "made keystore.jks"
fi
aapt package -f -M AndroidManifest.xml -S res -A assets -I $AJAR -F unsigned.apk
aapt add unsigned.apk classes.dex >/dev/null && rm -f classes.dex
zipalign -f 4 unsigned.apk aligned.apk
apksigner sign --ks keystore.jks --ks-pass env:TR_KS_PASS --key-pass env:TR_KS_PASS --out "$OUT" aligned.apk
rm -f unsigned.apk aligned.apk "$OUT.idsig"
apksigner verify "$OUT"
aapt dump badging "$OUT" | grep -E "^package|application-label:"
ls -la "$OUT"

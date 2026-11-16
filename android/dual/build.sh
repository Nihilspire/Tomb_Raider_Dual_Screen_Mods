#!/bin/bash
# Builds "Tomb Raider Dual <version>.apk" (from Xenogears Dual's build): aapt -> javac -> dx -> aapt -> zipalign -> apksigner.
# No Gradle. The Link page is ../../app/www, copied in as assets/link; nothing from the game is in the APK.
# Usage: ./build.sh   (version from AndroidManifest.xml; run build_jni.sh first when jni/psxlink.c changes)
set -e
cd "$(dirname "$0")"
VER=$(grep -o 'versionName="[^"]*"' AndroidManifest.xml | cut -d'"' -f2)
AJAR=/usr/lib/android-sdk/platforms/android-23/android.jar
JDK=/usr/lib/jvm/java-17-openjdk-amd64/bin
rm -rf gen obj dex unsigned.apk aligned.apk assets/link
mkdir -p gen obj dex
cp -r ../../app/www assets/link
sed -i "s/var VERSION='[^']*';/var VERSION='$VER';/" assets/link/index.html
aapt package -f -m -J gen -M AndroidManifest.xml -S res -I $AJAR
$JDK/javac --release 8 -nowarn -Xlint:-options -encoding UTF-8 -d obj -classpath $AJAR -sourcepath src:gen $(find src gen -name '*.java')
dalvik-exchange --dex --min-sdk-version=23 --output=dex/classes.dex obj
aapt package -f -M AndroidManifest.xml -S res -A assets -I $AJAR -F unsigned.apk
cp dex/classes.dex .
aapt add unsigned.apk classes.dex > /dev/null
rm classes.dex
( cd out && for f in lib/arm64-v8a/*.so; do aapt add -0 so ../unsigned.apk "$f" > /dev/null; done )
zipalign -f -p 4 unsigned.apk aligned.apk
mkdir -p signing
# The signing password is the one line of signing/tombraider.jks.pass, which stays out of git and out of source zips.
if [ ! -f signing/tombraider.jks ]; then
  [ -f signing/tombraider.jks.pass ] || { head -c 18 /dev/urandom | base64 | tr -d '/+=' > signing/tombraider.jks.pass; chmod 600 signing/tombraider.jks.pass; }
  P=$(cat signing/tombraider.jks.pass)
  keytool -genkeypair -keystore signing/tombraider.jks -alias tombraider -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass "$P" -keypass "$P" -dname "CN=Tomb Raider Dual, O=Jack" > /dev/null 2>&1
fi
[ -f signing/tombraider.jks.pass ] || { echo "missing signing/tombraider.jks.pass (the key's password, one line)"; exit 1; }
apksigner sign --ks signing/tombraider.jks --ks-pass file:signing/tombraider.jks.pass --ks-key-alias tombraider \
  --out "Tomb Raider Dual $VER.apk" aligned.apk
rm -f unsigned.apk aligned.apk
apksigner verify "Tomb Raider Dual $VER.apk" && echo "Built Tomb Raider Dual $VER.apk"

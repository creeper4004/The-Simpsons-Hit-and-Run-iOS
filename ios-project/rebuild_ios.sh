#!/bin/bash
# Rebuild MGL ios-sim libs + relink + install the iOS app.
# Usage: rebuild_ios.sh [--run]   (--run also launches on the booted sim)
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
STAGE="${MGL_STAGE_DIR:-$ROOT/mgl-ios/libs}"
APP="build_ios/code/Release-iphonesimulator/Simpsons.app"
BUNDLE="com.c4rlox.simpsons.ios"

echo "== [1/4] MGL libs =="
(cd "$ROOT/mgl-ios/iosbuild" && ./build.sh >/dev/null)

echo "== [2/4] cmake regen =="
cmake -B build_ios -DIOS_MGL_LIB_DIR="$STAGE" >/dev/null

echo "== [3/4] xcodebuild =="
rm -f "$APP/Simpsons"
xcodebuild -project build_ios/SRR2.xcodeproj -scheme SimpsonsIOS \
  -configuration Release -sdk iphonesimulator build 2>&1 | grep -E "error:|BUILD (SUCCEEDED|FAILED)"

if [ "$1" = "--run" ]; then
  echo "== [4/4] install + launch =="
  UDID=$(xcrun simctl list devices | grep -i booted | head -1 | grep -oE "[0-9A-F-]{36}" || true)
  if [ -z "$UDID" ]; then echo "no booted sim"; exit 1; fi
  xcrun simctl terminate "$UDID" "$BUNDLE" 2>/dev/null || true
  xcrun simctl install "$UDID" "$APP"
  LOG="/tmp/shar_ios_$(date +%H%M%S).log"
  nohup xcrun simctl launch --console-pty "$UDID" "$BUNDLE" > "$LOG" 2>&1 &
  echo "launched, log: $LOG"
fi

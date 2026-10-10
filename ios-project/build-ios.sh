#!/usr/bin/env bash
#=============================================================================
# build-ios.sh — configure + build the iOS port (engine static libs + app).
#
# Produces a runnable iOS Simulator .app:
#   build_ios/code/Release-iphonesimulator/Simpsons.app
# or the device build when SYSROOT=iphoneos.
#
# Environment:
#   CONFIG  Debug|Release                                    (default Release)
#   SYSROOT iphonesimulator|iphoneos                        (default iphonesimulator)
#   SRR2_IOS_EMBED_GAME_ASSETS ON|OFF (default OFF; data in Documents/game)
#   SRR2_REGION NTSC|PAL                                    (default PAL)
#   FFMPEG_OUT_DIR  path to the FFmpeg static libs output  (default $HOME/src/srr2_ios_builds/ffmpeg_out_sim)
#
# Prereqs: Xcode (with iOS SDK), CMake 3.20+, and the FFmpeg static libs built
# via ./build-ffmpeg.sh.
#=============================================================================
set -euo pipefail
cd "$(dirname "$0")"

CONFIG="${CONFIG:-Release}"
SYSROOT="${SYSROOT:-iphonesimulator}"
REGION="${SRR2_REGION:-PAL}"
EMBED_GAME_ASSETS="${SRR2_IOS_EMBED_GAME_ASSETS:-OFF}"
FFMPEG_OUT_DIR="${FFMPEG_OUT_DIR:-$HOME/src/srr2_ios_builds/ffmpeg_out_sim}"
BUILD_DIR="${BUILD_DIR:-../build_ios}"

if [[ "${SYSROOT}" == "iphonesimulator" ]]; then
    SDKBASE="iphonesimulator"
else
    SDKBASE="iphoneos"
fi

# Dispositivo real (iphoneos) necesita firma; simulador no.
SIGN_FLAG="-DSRR2_SIGN=OFF"
if [[ "${SDKBASE}" == "iphoneos" ]]; then
    SIGN_FLAG="-DSRR2_SIGN=ON"
fi

echo ">> iOS build: config=${CONFIG} sysroot=${SYSROOT} region=${REGION}"
echo ">> FFmpeg dir: ${FFMPEG_OUT_DIR}"

if [[ ! -f "${FFMPEG_OUT_DIR}/lib/libavcodec.a" ]]; then
    echo "!! Missing FFmpeg static libs in ${FFMPEG_OUT_DIR}/lib (no libavcodec.a)."
    echo "   Run ./build-ffmpeg.sh first."
    exit 1
fi

cmake -S .. -B "${BUILD_DIR}" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${SDKBASE}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DIOS=ON \
    -DSRR2_P3D_PDDI=GLES2 \
    -DSRR2_REGION="${REGION}" \
    -DSRR2_IOS_EMBED_GAME_ASSETS="${EMBED_GAME_ASSETS}" \
    -DSRR2_IOS_FFMPEG_DIR="${FFMPEG_OUT_DIR}" \
    "${SIGN_FLAG}" \
    -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TEST=OFF \
    -DALSOFT_TESTS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF

# Build via xcodebuild with an explicit destination so the SDK/arch match the
# requested SYSROOT (device vs simulator). `cmake --build` alone defaults to the
# device, which fails for a simulator-only FFmpeg/OpenAL build.
if [[ "${SYSROOT}" == "iphoneos" ]]; then
    DEST="generic/platform=iOS"
else
    DEST="generic/platform=iOS Simulator"
fi
xcodebuild \
    -project "${BUILD_DIR}/SRR2.xcodeproj" \
    -scheme SimpsonsIOS \
    -configuration "${CONFIG}" \
    -sdk "${SDKBASE}" \
    -destination "${DEST}" \
    build

APP="${BUILD_DIR}/code/${CONFIG}-${SDKBASE}/Simpsons.app"
echo
echo ">> Built: ${APP}"

#!/usr/bin/env bash
#=============================================================================
# build-ffmpeg.sh — builds FFmpeg 6.0 static libraries for iOS.
#
# The Android port links FFmpeg via the "com.fpliu.ndk..." Prefab AAR, but
# there is no equivalent iOS AAR. This script produces the same FFmpeg 6.0
# feature set (LGPL) as static .a archives for the iOS Simulator (arm64) and
# places them where ios-project/CMake expects them:
#
#     $OUT_DIR/lib/{libavcodec,libavformat,libavutil,libswresample,libswscale}.a
#     $OUT_DIR/include/{libavcodec,libavformat,libavutil,libswresample,libswscale}/
#
# Usage:
#   ./ios-project/build-ffmpeg.sh                     # builds into $HOME/src/srr2_ios_builds/ffmpeg_out_sim
#   OUT_DIR=/path/out ./ios-project/build-ffmpeg.sh    # explicit output dir
#
# Pass the matching directory to CMake:
#   cmake -DIOS=ON -DSRR2_IOS_FFMPEG_DIR=$OUT_DIR ...
#=============================================================================
set -euo pipefail

FF_VERSION="6.0"
SRC_DIR="${FFMPEG_SRC_DIR:-$HOME/src/srr2_ios_builds/ffmpeg_src/ffmpeg-${FF_VERSION}}"
OUT_DIR="${OUT_DIR:-$HOME/src/srr2_ios_builds/ffmpeg_out_sim}"

XC="$(xcrun --find clang)"
ARCH="arm64"
MIN_IOS="15.0"

# PLATFORM: "sim" (default, iOS Simulator) or "device" (iphoneos/real device).
# FFmpeg infers iOS vs iOS-simulator from the -target triple, NOT from
# -miphoneos/-mios-simulator-version-min. We pass the full target so clang
# emits the correct LC_BUILD_VERSION platform (device=2, simulator=7).
PLATFORM="${PLATFORM:-sim}"
if [[ "${PLATFORM}" == "device" ]]; then
    SDK="${SDK:-$(xcrun --sdk iphoneos --show-sdk-path)}"
    TRIPLE="arm64-apple-ios${MIN_IOS}"
    VFLAG="-target ${TRIPLE}"
else
    SDK="${SDK:-$(xcrun --sdk iphonesimulator --show-sdk-path)}"
    TRIPLE="arm64-apple-ios${MIN_IOS}-simulator"
    VFLAG="-target ${TRIPLE}"
fi

echo ">> FFmpeg ${FF_VERSION} static build for iOS ${PLATFORM} (${ARCH}, min ${MIN_IOS})"
echo "   src:   ${SRC_DIR}"
echo "   out:   ${OUT_DIR}"
echo "   sdk:   ${SDK}"
echo

if [[ ! -d "${SRC_DIR}" ]]; then
    echo ">> FFmpeg source not found: ${SRC_DIR}"
    echo "   Download it first:"
    echo "     mkdir -p $HOME/src/srr2_ios_builds/ffmpeg_src && cd $HOME/src/srr2_ios_builds/ffmpeg_src"
    echo "     curl -sL https://ffmpeg.org/releases/ffmpeg-${FF_VERSION}.tar.xz -o ffmpeg-${FF_VERSION}.tar.xz"
    echo "     tar xf ffmpeg-${FF_VERSION}.tar.xz"
    exit 1
fi

cd "${SRC_DIR}"

# NOTE: "ffmpeg" on iOS is deliberately broad-but-lean. The game's FMVs (.rmv /
# .mov via radmovie) only need the mov/rm/ivf demuxers and the H.264 / MPEG-4 /
# MPEG-2 video decoders plus the PCM audio decoders used by the engine. The
# "--disable-everything" base keeps the binary small (GPL-free / LGPL build).
./configure \
    --prefix="${OUT_DIR}" \
    --target-os=darwin \
    --arch="${ARCH}" \
    --cc="${XC}" \
    --enable-cross-compile \
    --sysroot="${SDK}" \
    --extra-cflags="${VFLAG} -isysroot ${SDK}" \
    --extra-ldflags="${VFLAG} -isysroot ${SDK}" \
    --enable-static --disable-shared \
    --enable-pic \
    --disable-doc --disable-programs --disable-avdevice --disable-postproc \
    --disable-network --disable-everything \
    --enable-demuxer=mov --enable-demuxer=rm --enable-demuxer=ivf \
    --enable-decoder=h264 --enable-decoder=mpeg4 --enable-decoder=mpeg2video \
    --enable-decoder=rawvideo --enable-decoder=pcm_s16le --enable-decoder=pcm_mulaw --enable-decoder=pcm_alaw \
    --enable-decoder=mjpeg --enable-decoder=png \
    --enable-protocol=file \
    --enable-swscale --enable-swresample

make -j"$(sysctl -n hw.ncpu)"
make install

echo
echo ">> Done. Libraries in: ${OUT_DIR}/lib"
echo ">> Headers in:         ${OUT_DIR}/include"
echo ">> Point CMake at it:  -DSRR2_IOS_FFMPEG_DIR=${OUT_DIR}"

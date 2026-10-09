#!/bin/bash
# Build MGL archives. MGL_SDK=iphoneos selects isolated device outputs;
# the default simulator archives remain in ./libs:
#   libmgl.a libglslang.a libSPIRV.a libSPIRV-Tools.a libglslang-default-resource-limits.a
#
# Sources: if ./src/mgl is missing, fetches pinned MGL + the external
# shader-toolchain repos (glslang, SPIRV-Tools, SPIRV-Cross, SPIRV-Headers)
# and applies ../mgl-ios.patch. Network only needed for the first build.
set -euo pipefail
cd "$(dirname "$0")"          # iosbuild/
ROOT="$(pwd)/.."              # mgl-ios/
SRC="$ROOT/src/mgl"
MGL_SDK="${MGL_SDK:-iphonesimulator}"
case "$MGL_SDK" in
  iphonesimulator)
    STAGE="${MGL_STAGE_DIR:-$ROOT/libs}"
    BUILD_ROOT="$SRC/build"
    TC="$ROOT/iosbuild/ios-sim.toolchain.cmake"
    ;;
  iphoneos)
    STAGE="${MGL_STAGE_DIR:-$ROOT/work/iphoneos/libs}"
    BUILD_ROOT="$SRC/build/device"
    TC="$ROOT/iosbuild/ios-device.toolchain.cmake"
    ;;
  *) echo "Unsupported MGL_SDK: $MGL_SDK" >&2; exit 1 ;;
esac
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
mkdir -p "$STAGE"
STAGE="$(cd "$STAGE" && pwd)"

# Fast path: committed libs already staged. FORCE=1 to rebuild from source.
if [ -z "${FORCE:-}" ] && ls "$STAGE"/libmgl.a "$STAGE"/libglslang.a "$STAGE"/libSPIRV.a    "$STAGE"/libSPIRV-Tools.a "$STAGE"/libglslang-default-resource-limits.a >/dev/null 2>&1; then
  echo "libs present in $STAGE (FORCE=1 rebuilds from source)"
  exit 0
fi

# Keep source recovery reproducible. Do not reset an existing working checkout:
# it can contain local rendering experiments not yet saved in mgl-ios.patch.
fetch_revision() {
  local destination="$1" url="$2" revision="$3"
  if [ ! -d "$destination/.git" ]; then
    git init -q "$destination"
    git -C "$destination" remote add origin "$url"
    git -C "$destination" fetch --depth 1 origin "$revision"
    git -C "$destination" checkout --detach FETCH_HEAD
  fi
}

if [ ! -d "$SRC/MGL" ]; then
  echo "== fetching pinned MGL into $SRC =="
  fetch_revision "$SRC" https://github.com/openglonmetal/MGL.git de0ded04ec7dc99182e27e555a17775523b26911
  # Fail the build if the portability/rendering patch cannot be applied.
  git -C "$SRC" apply "$ROOT/mgl-ios.patch"
fi

fetch_revision "$SRC/external/SPIRV-Cross" https://github.com/r58Playz/SPIRV-Cross.git 1884e758a2ccddc3b3ab7e8ce9511400f86f58fd
fetch_revision "$SRC/external/SPIRV-Headers" https://github.com/KhronosGroup/SPIRV-Headers.git cb42dec3830d3ac67fa449ecdc0c0f73d5e74498
fetch_revision "$SRC/external/SPIRV-Tools" https://github.com/KhronosGroup/SPIRV-Tools.git 1d0401cd2b68ae34cda9ff625bedd1be4ed6214a
fetch_revision "$SRC/external/glslang" https://github.com/KhronosGroup/glslang.git 2ff6f609379ce43c4291c732cf6a19dd2461a680
mkdir -p "$SRC/external/SPIRV-Tools/external"
if [ ! -e "$SRC/external/SPIRV-Tools/external/SPIRV-Headers" ]; then
  ln -s ../../SPIRV-Headers "$SRC/external/SPIRV-Tools/external/SPIRV-Headers"
fi

# Refresh the wrapper on incremental builds as well as fresh checkouts.
mkdir -p "$SRC/iosbuild"
cp "$ROOT/iosbuild/CMakeLists.txt" "$ROOT/iosbuild/glm_params_ios.c" "$ROOT/iosbuild/shader_interface.h" "$SRC/iosbuild/"
cp -R "$ROOT/iosbuild/shim" "$SRC/iosbuild/"

cd "$SRC"
export MGL_STAGE="$STAGE"

echo "== SPIRV-Tools =="
cmake -S external/SPIRV-Tools -B "$BUILD_ROOT/spirv-tools" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" \
  -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_WERROR=OFF >/dev/null
cmake --build "$BUILD_ROOT/spirv-tools" -j "$JOBS" >/dev/null
find "$BUILD_ROOT/spirv-tools" -name "libSPIRV-Tools*.a" -exec cp {} "$STAGE/libSPIRV-Tools.a" \; -quit

echo "== glslang =="
cmake -S external/glslang -B "$BUILD_ROOT/glslang" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" \
  -DENABLE_OPT=OFF -DBUILD_TESTING=OFF -DENABLE_HLSL=OFF \
  -DENABLE_GLSLANG_BINARIES=ON -DGLSLANG_TESTS=OFF >/dev/null
cmake --build "$BUILD_ROOT/glslang" -j "$JOBS" >/dev/null
find "$BUILD_ROOT/glslang" -name "libglslang.a" -exec cp {} "$STAGE/libglslang.a" \; -quit
find "$BUILD_ROOT/glslang" -name "libSPIRV.a" -exec cp {} "$STAGE/libSPIRV.a" \; -quit
find "$BUILD_ROOT/glslang" -name "libglslang-default-resource-limits.a" -exec cp {} "$STAGE/libglslang-default-resource-limits.a" \; -quit

echo "== SPIRV-Cross =="
cmake -S external/SPIRV-Cross -B "$BUILD_ROOT/spirv-cross" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_ENABLE_C_API=OFF \
  -DSPIRV_CROSS_CLI=OFF -DSPIRV_CROSS_ENABLE_HLSL=OFF \
  -DSPIRV_CROSS_ENABLE_REFLECT=ON -DSPIRV_CROSS_ENABLE_CPP=ON \
  -DSPIRV_CROSS_ENABLE_UTIL=OFF >/dev/null
cmake --build "$BUILD_ROOT/spirv-cross" -j "$JOBS" >/dev/null
mkdir -p "$STAGE/spirv-cross"
for l in core msl glsl reflect cpp; do
  find "$BUILD_ROOT/spirv-cross" -name "libspirv-cross-$l.a" -exec cp {} "$STAGE/spirv-cross/" \; -quit
done

echo "== MGL core =="
cmake -S iosbuild -B "$BUILD_ROOT/mgl" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" -DMGL_STAGE="$STAGE" >/dev/null
cmake --build "$BUILD_ROOT/mgl" -j "$JOBS"
echo "== staged =="
ls -la "$STAGE"/*.a

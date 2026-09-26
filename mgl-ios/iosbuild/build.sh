#!/bin/bash
# Build the 5 MGL iOS-simulator static libs the engine links into ./libs:
#   libmgl.a libglslang.a libSPIRV.a libSPIRV-Tools.a libglslang-default-resource-limits.a
#
# Sources: if ./src/mgl is missing, fetches MGL master + the external
# shader-toolchain repos (glslang, SPIRV-Tools, SPIRV-Cross, SPIRV-Headers)
# and applies ../mgl-ios.patch. Network only needed for the first build.
set -e
cd "$(dirname "$0")"          # iosbuild/
ROOT="$(pwd)/.."              # mgl-ios/
SRC="$ROOT/src/mgl"
STAGE="$ROOT/libs"
TC="$ROOT/iosbuild/ios-sim.toolchain.cmake"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
mkdir -p "$STAGE"

# Fast path: committed libs already staged. FORCE=1 to rebuild from source.
if [ -z "$FORCE" ] && ls "$STAGE"/libmgl.a "$STAGE"/libglslang.a "$STAGE"/libSPIRV.a    "$STAGE"/libSPIRV-Tools.a "$STAGE"/libglslang-default-resource-limits.a >/dev/null 2>&1; then
  echo "libs present in $STAGE (FORCE=1 rebuilds from source)"
  exit 0
fi

if [ ! -d "$SRC/MGL" ]; then
  echo "== fetching MGL master + externals into $SRC =="
  mkdir -p "$SRC"
  git clone --depth 1 https://github.com/openglonmetal/MGL.git "$SRC" 2>/dev/null || true
  cd "$SRC"
  git apply "$ROOT/mgl-ios.patch" || echo "patch failed (already applied? check $SRC/MGL)"
  # iosbuild wrapper (CMake target + iOS caps stub + mach shim) must live in
  # the source root, mirroring the original /tmp layout.
  mkdir -p iosbuild
  cp "$ROOT/iosbuild/CMakeLists.txt" "$ROOT/iosbuild/glm_params_ios.c" iosbuild/
  cp -r "$ROOT/iosbuild/shim" iosbuild/
  cd external
  for r in "SPIRV-Cross:https://github.com/r58Playz/SPIRV-Cross.git -b uniform-constants" \
           "SPIRV-Headers:https://github.com/KhronosGroup/SPIRV-Headers.git" \
           "SPIRV-Tools:https://github.com/KhronosGroup/SPIRV-Tools.git" \
           "glslang:https://github.com/KhronosGroup/glslang.git"; do
    name="${r%%:*}"; url="${r#*:}"
    [ -d "$name" ] || git clone --depth 1 $url "$name" >/dev/null 2>&1
  done
  # glslang/SPIRV-Cross also need SPIRV-Headers on the include path
  mkdir -p SPIRV-Tools/external
  [ -e SPIRV-Tools/external/SPIRV-Headers ] || ln -sfn ../../SPIRV-Headers SPIRV-Tools/external/SPIRV-Headers
  cd "$ROOT"
fi

cd "$SRC"
export MGL_STAGE="$STAGE"
TC="$SRC/iosbuild/ios-sim.toolchain.cmake" 2>/dev/null || TC="$ROOT/iosbuild/ios-sim.toolchain.cmake"
[ -f "$TC" ] || TC="$ROOT/iosbuild/ios-sim.toolchain.cmake"

echo "== SPIRV-Tools =="
cmake -S external/SPIRV-Tools -B build/spirv-tools -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" \
  -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_WERROR=OFF >/dev/null
cmake --build build/spirv-tools -j "$JOBS" >/dev/null
find build/spirv-tools -name "libSPIRV-Tools*.a" -exec cp {} "$STAGE/libSPIRV-Tools.a" \; -quit

echo "== glslang =="
cmake -S external/glslang -B build/glslang -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" \
  -DENABLE_OPT=OFF -DBUILD_TESTING=OFF -DENABLE_HLSL=OFF \
  -DENABLE_GLSLANG_BINARIES=ON -DGLSLANG_TESTS=OFF >/dev/null
cmake --build build/glslang -j "$JOBS" >/dev/null
find build/glslang -name "libglslang.a" -exec cp {} "$STAGE/libglslang.a" \; -quit
find build/glslang -name "libSPIRV.a" -exec cp {} "$STAGE/libSPIRV.a" \; -quit
find build/glslang -name "libglslang-default-resource-limits.a" -exec cp {} "$STAGE/libglslang-default-resource-limits.a" \; -quit

echo "== SPIRV-Cross =="
cmake -S external/SPIRV-Cross -B build/spirv-cross -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_ENABLE_C_API=OFF \
  -DSPIRV_CROSS_CLI=OFF -DSPIRV_CROSS_ENABLE_HLSL=OFF \
  -DSPIRV_CROSS_ENABLE_REFLECT=ON -DSPIRV_CROSS_ENABLE_CPP=ON \
  -DSPIRV_CROSS_ENABLE_UTIL=OFF >/dev/null
cmake --build build/spirv-cross -j "$JOBS" >/dev/null
mkdir -p "$STAGE/spirv-cross"
for l in core msl glsl reflect cpp; do
  find build/spirv-cross -name "libspirv-cross-$l.a" -exec cp {} "$STAGE/spirv-cross/" \; -quit
done

echo "== MGL core =="
cmake -S iosbuild -B build/mgl -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TC" -DMGL_STAGE="$STAGE" >/dev/null
cmake --build build/mgl -j "$JOBS"
echo "== staged =="
ls -la "$STAGE"/*.a

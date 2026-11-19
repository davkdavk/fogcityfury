#!/bin/bash
# Build the real game as an xsdk .xex. Needs the xsdk source/build and its compiler.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
XSDK="${XSDK:-$HOME/xsdk}"
TOOLCHAIN="${XSDK_TOOLCHAIN:-$HOME/xsdk-gcc}"
export XSDK
WATER_TEST="${FCF_WATER_TEST:-0}"
case "$WATER_TEST" in
  0|1|2|3) ;;
  *) echo "FCF_WATER_TEST must be 0, 1, 2, or 3" >&2; exit 1 ;;
esac
export FCF_WATER_TEST="$WATER_TEST"
CULL_TEST="${FCF_XSDK_CULL_TEST:-OFF}"
case "$CULL_TEST" in
  ON|OFF) ;;
  *) echo "FCF_XSDK_CULL_TEST must be ON or OFF" >&2; exit 1 ;;
esac
PERF_TEST="${FCF_PERF_TEST:-0}"
if [ "$CULL_TEST" = ON ]; then
  if [ "$PERF_TEST" = 0 ]; then PERF_TEST=flat; fi
  if [ "$PERF_TEST" != flat ]; then echo "FCF_XSDK_CULL_TEST requires FCF_PERF_TEST=flat" >&2; exit 1; fi
fi
case "$PERF_TEST" in
  0|flat|noise|lean) ;;
  *) echo "FCF_PERF_TEST must be 0, flat, noise, or lean" >&2; exit 1 ;;
esac
export FCF_PERF_TEST="$PERF_TEST"
if [ "$WATER_TEST" != 0 ] && [ "$PERF_TEST" != 0 ]; then
  echo "FCF_WATER_TEST and FCF_PERF_TEST cannot be combined" >&2
  exit 1
fi
PATCH="$HERE/patches/xsdk-display-lut.patch"
if git -C "$XSDK" apply --unidiff-zero --check "$PATCH" 2>/dev/null; then
  git -C "$XSDK" apply --unidiff-zero "$PATCH"
elif ! git -C "$XSDK" apply --unidiff-zero --reverse --check "$PATCH" 2>/dev/null; then
  echo "cannot apply $PATCH to $XSDK" >&2
  exit 1
fi
"$HERE/build_shaders.sh"
OBJECT_TEST="${XSDK_OBJECT_TEST:-OFF}"
DRAW_MASKS="${XSDK_DRAW_MASKS:-OFF}"
BUILD_DIR="$HERE/build/game"
if [ "$WATER_TEST" != 0 ]; then BUILD_DIR="$HERE/build/water_test_$WATER_TEST"; fi
if [ "$PERF_TEST" != 0 ]; then BUILD_DIR="$HERE/build/perf_$PERF_TEST"; fi
if [ "$CULL_TEST" = ON ]; then BUILD_DIR="$HERE/build/cull_test"; fi
cmake --fresh -S "$HERE" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release -DFCF_XSDK_OBJECT_TEST="$OBJECT_TEST" -DFCF_XSDK_DRAW_MASKS="$DRAW_MASKS" -DFCF_XSDK_CULL_TEST="$CULL_TEST" -DFCF_WATER_TEST="$WATER_TEST" -DFCF_PERF_TEST="$PERF_TEST"
cmake --build "$BUILD_DIR"
cp "$HERE/dbgprint.txt" "$BUILD_DIR/dbgprint.txt"
if [ "$WATER_TEST" != 0 ]; then
  mkdir -p "$HERE/build/game"
  cp "$BUILD_DIR/water_test_$WATER_TEST.xex" "$HERE/build/game/"
  cp "$HERE/dbgprint.txt" "$HERE/build/game/dbgprint.txt"
fi
if [ "$PERF_TEST" != 0 ] && [ "$CULL_TEST" != ON ]; then
  mkdir -p "$HERE/build/game"
  if [ "$PERF_TEST" = lean ]; then cp "$BUILD_DIR"/lean*.xex "$HERE/build/game/"; else cp "$BUILD_DIR/perf_$PERF_TEST.xex" "$HERE/build/game/"; fi
  cp "$HERE/dbgprint.txt" "$HERE/build/game/dbgprint.txt"
fi
if [ "$CULL_TEST" = ON ]; then
  mkdir -p "$HERE/build/game"
  cp "$BUILD_DIR"/cull_*.xex "$HERE/build/game/"
  cp "$HERE/dbgprint.txt" "$HERE/build/game/dbgprint.txt"
fi

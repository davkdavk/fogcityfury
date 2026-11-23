#!/bin/bash
# Build the game for the Xbox 360 as build/game/lean.xex (with dbgprint.txt). Needs xsdk and its compiler:
#   XSDK=~/xsdk XSDK_TOOLCHAIN=~/xsdk-gcc xenon/build_game.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
XSDK="${XSDK:-$HOME/xsdk}"
TOOLCHAIN="${XSDK_TOOLCHAIN:-$HOME/xsdk-gcc}"
export XSDK
# (FCF_PERF_TEST=lean is what this always builds now; the old diagnostic switches are gone)
for v in FCF_WATER_TEST FCF_XSDK_CULL_TEST FCF_LEAN_DEBUG XSDK_OBJECT_TEST XSDK_DRAW_MASKS FCF_XSDK_LEAN_MASK23 FCF_XSDK_LEAN_NOPREPASS \
         FCF_XSDK_LEAN_SKYFIRST FCF_XSDK_LEAN_NOEFFECTS FCF_XSDK_LEAN_DEPTHFILL FCF_XSDK_LEAN_SKY_BEHIND FCF_XSDK_LEAN_NOHIZ; do
  val="${!v:-}"
  if [ -n "$val" ] && [ "$val" != 0 ] && [ "$val" != OFF ]; then echo "$v: the diagnostic 360 builds were removed in 2.25" >&2; exit 1; fi
done
if [ -n "${FCF_PERF_TEST:-}" ] && [ "${FCF_PERF_TEST}" != lean ] && [ "${FCF_PERF_TEST}" != 0 ]; then echo "FCF_PERF_TEST: only the lean build is left (2.25)" >&2; exit 1; fi
SHADER_DIR="$HERE/build/gen"
export FCF_SHADER_OUT="$SHADER_DIR"
PATCH="$HERE/patches/xsdk-display-lut.patch"
if git -C "$XSDK" apply --unidiff-zero --check "$PATCH" 2>/dev/null; then
  git -C "$XSDK" apply --unidiff-zero "$PATCH"
elif ! git -C "$XSDK" apply --unidiff-zero --reverse --check "$PATCH" 2>/dev/null; then
  echo "cannot apply $PATCH to $XSDK" >&2
  exit 1
fi
"$HERE/build_shaders.sh"
BUILD_DIR="$HERE/build/perf_lean"
cmake --fresh -S "$HERE" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release -DFCF_SHADER_DIR="$SHADER_DIR"
cmake --build "$BUILD_DIR"
mkdir -p "$HERE/build/game"
cp "$BUILD_DIR/lean.xex" "$HERE/build/game/"
cp "$HERE/dbgprint.txt" "$HERE/build/game/dbgprint.txt"

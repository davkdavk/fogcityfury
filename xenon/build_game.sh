#!/bin/bash
# Build the real game as an xsdk .xex. Needs the xsdk source/build and its compiler.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
XSDK="${XSDK:-$HOME/xsdk}"
TOOLCHAIN="${XSDK_TOOLCHAIN:-$HOME/xsdk-gcc}"
export XSDK
PATCH="$HERE/patches/xsdk-display-lut.patch"
if git -C "$XSDK" apply --unidiff-zero --check "$PATCH" 2>/dev/null; then
  git -C "$XSDK" apply --unidiff-zero "$PATCH"
elif ! git -C "$XSDK" apply --unidiff-zero --reverse --check "$PATCH" 2>/dev/null; then
  echo "cannot apply $PATCH to $XSDK" >&2
  exit 1
fi
"$HERE/build_shaders.sh"
OBJECT_TEST="${XSDK_OBJECT_TEST:-OFF}"
cmake --fresh -S "$HERE" -B "$HERE/build/game" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release -DFCF_XSDK_OBJECT_TEST="$OBJECT_TEST"
cmake --build "$HERE/build/game"
cp "$HERE/dbgprint.txt" "$HERE/build/game/dbgprint.txt"

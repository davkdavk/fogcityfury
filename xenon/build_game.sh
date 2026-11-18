#!/bin/bash
# Build the real game as an xsdk .xex. Needs the xsdk source/build and its compiler.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
XSDK="${XSDK:-/tmp/xsdk}"
TOOLCHAIN="${XSDK_TOOLCHAIN:-/var/tmp/xsdk-gcc}"
export XSDK
"$HERE/build_shaders.sh"
cmake -S "$HERE" -B "$HERE/build/game" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build/game"

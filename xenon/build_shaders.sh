#!/bin/bash
# One command: transform the game's GLSL and compile all ten programs (20 stages) to Xenos microcode.
# Needs: glslang on PATH, and xsdk (XSDK env, default /tmp/xsdk) for xsc. Run from anywhere:
#   XSDK=/path/to/xsdk bash xenon/build_shaders.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(dirname "$HERE")"
XSDK="${XSDK:-/tmp/xsdk}"
XSC="$XSDK/xenos-toolchain"
GEN="$HERE/build/gen"
[ -d "$XSC" ] || { echo "xsdk not found at $XSC (set XSDK)"; exit 1; }
python3 "$HERE/transform.py" "$GEN" || exit 1
fail=0
for p in world car water sky part depth ui mark ring post; do
  for st in vert frag; do
    f="$GEN/$p.$st"
    if ! glslang -V "$f" -o "$f.spv" 2>"$f.glslang.log"; then
      echo "$p.$st GLSL-FAIL: $(grep -i error "$f.glslang.log" | head -1)"; fail=1; continue
    fi
    if (cd "$XSC" && python3 -m xsc.lower "$f.spv" --pixel-registers 64 --vertex-registers 64 --optimize -o "$f.bin" >"$f.xsc.log" 2>&1); then
      echo "$p.$st OK ($(stat -c%s "$f.bin")B)"
    else
      echo "$p.$st XSC-FAIL: $(grep -iE 'Error|refus|register' "$f.xsc.log" | tail -1)"; fail=1
    fi
  done
done
[ $fail -eq 0 ] && echo "ALL 20 STAGES COMPILED" || echo "SOME STAGES FAILED"
[ $fail -eq 0 ] || exit $fail
python3 "$HERE/pack_shaders.py" "$GEN" || exit 1
exit $fail

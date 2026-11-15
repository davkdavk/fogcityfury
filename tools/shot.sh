#!/bin/sh
# Render one screenshot without a display (needs xvfb-run and Mesa's software renderer).
#   tools/shot.sh NAME [game flags...]      -> shots/NAME.png (or .bmp if ImageMagick is missing)
# Examples:
#   tools/shot.sh title --state 0 --skip 300
#   tools/shot.sh race  --state 3 --auto --skip 700
#   tools/shot.sh map   --state 3 --mode 1 --top 900 0 0 --noui --size 1000 1000
# The game prints its JSON state report on stdout (see docs/TESTING.md).
set -e
cd "$(dirname "$0")/.."
n=$1; shift
mkdir -p shots
xvfb-run -a -s "-screen 0 1280x720x24" ./fogcityfury --nosave --shot "shots/$n.bmp" "$@"
if command -v convert >/dev/null 2>&1; then convert "shots/$n.bmp" "shots/$n.png" && rm "shots/$n.bmp"; fi

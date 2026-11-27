#!/bin/sh
# lobbies.sh - eight Fog City Fury lobbies (10 players each) from this folder, using the server beside this script.
# Lobby 1 on UDP 27015, lobby 2 on 27016 ... lobby 8 on 27022. Each restarts itself if it stops.
#
#   ./lobbies.sh start       start them (any already running are left alone)
#   ./lobbies.sh stop        stop them
#   ./lobbies.sh restart     stop, then start (after copying in a new server)
#   ./lobbies.sh status      each lobby: running or not, its port, how many have joined
#   ./lobbies.sh boot        start them when the machine starts (adds a line to your crontab)
#
# Each lobby keeps its own uploaded tracks and its log in lobbyN/ here. Event files in events/ here (trackN.fcfe)
# go to every lobby. Settings: FCF_BIN (the server, default ./FogCityFury-Server-ARM64), FCF_LOBBIES (8),
# FCF_NAME (THADAV.COM: lobbies are "THADAV.COM 1" ...), FCF_EXTRA (more server flags, e.g. "--laps 4").
cd "$(dirname "$0")" || exit 1
HERE=$(pwd)
BIN=${FCF_BIN:-./FogCityFury-Server-ARM64}
N=${FCF_LOBBIES:-8}
BASE=27015
NAME=${FCF_NAME:-THADAV.COM}
EXTRA=${FCF_EXTRA:-}

running() { [ -f "lobby$1/pid" ] && kill -0 "$(cat "lobby$1/pid")" 2>/dev/null; }

start() {
    [ -x "$BIN" ] || { [ -f "$BIN" ] && chmod +x "$BIN"; } || true
    [ -x "$BIN" ] || { echo "no server at $BIN (set FCF_BIN to its name)"; exit 1; }
    i=1
    while [ "$i" -le "$N" ]; do
        if running "$i"; then echo "lobby $i: already running"; else
            mkdir -p "lobby$i/tracks"; port=$((BASE + i - 1))
            # a small loop round the server: if it ever stops, it starts again 3 s later; setsid gives the loop its own group, so stop ends both
            setsid sh -c 'while :; do "$0" --port "$1" --name "$2" --tracks "$3" $4; echo "lobby stopped; starting again in 3 s"; sleep 3; done' \
                "$BIN" "$port" "$NAME $i" "$HERE/lobby$i/tracks" "$EXTRA" >> "lobby$i/log.txt" 2>&1 < /dev/null &
            echo $! > "lobby$i/pid"; echo "lobby $i: started on UDP $port"
        fi
        i=$((i + 1))
    done
}

stop() {
    for d in lobby*/; do
        i=${d#lobby}; i=${i%/}
        if running "$i"; then kill -- "-$(cat "lobby$i/pid")" 2>/dev/null || kill "$(cat "lobby$i/pid")"; echo "lobby $i: stopped"; fi
        rm -f "lobby$i/pid"
    done
}

case "${1:-}" in
start) start ;;
stop) stop ;;
restart) stop; sleep 1; start ;;
status)
    i=1
    while [ "$i" -le "$N" ]; do
        s=stopped; running "$i" && s=running
        j=$(grep -c "joined" "lobby$i/log.txt" 2>/dev/null); [ -n "$j" ] || j=0
        printf 'lobby %s  UDP %s  %-8s  %s joins so far\n' "$i" "$((BASE + i - 1))" "$s" "$j"
        i=$((i + 1))
    done ;;
boot)
    line="@reboot $HERE/lobbies.sh start"
    ( crontab -l 2>/dev/null | grep -v "lobbies.sh start"; echo "$line" ) | crontab - && echo "they will start when the machine starts ($line)" ;;
*) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//' ;;
esac

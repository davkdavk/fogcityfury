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
# go to every lobby. Browser players: if FogCityFury-WSBridge-ARM64 is here too, it is started with the lobbies
# (TCP 27080, for the web server to pass wss://SITE/fcf/lobby/PORT to; see docs/WEB.md), its log in bridge/. Settings: FCF_BIN (the server, default ./FogCityFury-Server-ARM64), FCF_LOBBIES (8),
# FCF_NAME (THADAV.COM: lobbies are "THADAV.COM 1" ...), FCF_EXTRA (more server flags for every lobby).
# Each lobby's own settings: lobbies.conf here, a line per lobby, its number then its server flags, e.g.
#   1  --track 0 --fixed-track
#   2  --track 3 --weather random --laps 5
#   8  --roam --name "THADAV.COM ROAM"
# (--track 0-6, --fixed-track, --weather 0-3|random, --laps N, --bots 0-4, --no-showdown, --roam, --name NAME ...:
# ./FogCityFury-Server-ARM64 --help lists them). A lobby with no line gets the defaults. Then ./lobbies.sh restart.
cd "$(dirname "$0")" || exit 1
HERE=$(pwd)
BIN=${FCF_BIN:-./FogCityFury-Server-ARM64}
BRIDGE=${FCF_BRIDGE:-./FogCityFury-WSBridge-ARM64}
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
            own=$( [ -f lobbies.conf ] && sed -n "s/^[[:space:]]*$i[[:space:]][[:space:]]*//p" lobbies.conf | head -1 )   # (this lobby's line)
            ev=""; [ -d events ] && ev="--events '$HERE/events'"   # (the event files every lobby sends)
            # a small loop round the server: if it ever stops, it starts again 3 s later (setsid: its own group, so stop ends both).
            # eval reads the flags as a shell would, so a quoted --name "MY LOBBY" in lobbies.conf stays one name
            setsid sh -c 'B=$0 P=$1 L=$2 T=$3 F="$4 $5"; while :; do eval "\"\$B\" --port \"\$P\" --name \"\$L\" --tracks \"\$T\" $F"; echo "lobby stopped; starting again in 3 s"; sleep 3; done' \
                "$BIN" "$port" "$NAME $i" "$HERE/lobby$i/tracks" "$ev $EXTRA" "$own" >> "lobby$i/log.txt" 2>&1 < /dev/null &
            echo $! > "lobby$i/pid"; echo "lobby $i: started on UDP $port${own:+ ($own)}"
        fi
        i=$((i + 1))
    done
    if [ -f "$BRIDGE" ]; then   # the bridge for browser players
        [ -x "$BRIDGE" ] || chmod +x "$BRIDGE"
        if [ -f bridge/pid ] && kill -0 "$(cat bridge/pid)" 2>/dev/null; then echo "bridge: already running"; else
            mkdir -p bridge
            setsid sh -c 'while :; do "$0" --ports "$1" $2; echo "bridge stopped; starting again in 3 s"; sleep 3; done' \
                "$BRIDGE" "$BASE-$((BASE + N - 1))" "${FCF_BRIDGE_EXTRA:-}" >> bridge/log.txt 2>&1 < /dev/null &
            echo $! > bridge/pid; echo "bridge: started on TCP 27080 (browser players, through the web server)"
        fi
    fi
}

stop() {
    if [ -f bridge/pid ]; then kill -- "-$(cat bridge/pid)" 2>/dev/null || kill "$(cat bridge/pid)" 2>/dev/null; rm -f bridge/pid; echo "bridge: stopped"; fi
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
    done
    if [ -f "$BRIDGE" ]; then s=stopped; [ -f bridge/pid ] && kill -0 "$(cat bridge/pid)" 2>/dev/null && s=running
        j=$(grep -c "connected to lobby" bridge/log.txt 2>/dev/null); [ -n "$j" ] || j=0; printf 'bridge   TCP 27080  %-8s  %s browser connections so far\n' "$s" "$j"; fi ;;
boot)
    line="@reboot $HERE/lobbies.sh start"
    ( crontab -l 2>/dev/null | grep -v "lobbies.sh start"; echo "$line" ) | crontab - && echo "they will start when the machine starts ($line)" ;;
*) sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//' ;;
esac

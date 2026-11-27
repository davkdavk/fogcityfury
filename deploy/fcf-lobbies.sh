#!/bin/sh
# fcf-lobbies.sh - runs Fog City Fury lobbies on a Linux machine with systemd: N dedicated servers, 10 players each,
# lobby 1 on UDP 27015, lobby 2 on 27016, and so on. Each is a service that starts with the machine and comes back if
# it stops (deploy/fogcityfury-lobby@.service), as its own unprivileged user, sandboxed.
#
#   sudo ./fcf-lobbies.sh install [N] [SERVER]   set up and start N lobbies (default 8); SERVER: the fogcityfury-server
#                                                binary (default: ./fogcityfury-server, then ../fogcityfury-server)
#   sudo ./fcf-lobbies.sh update SERVER          a new server binary into every lobby, then restart them
#   sudo ./fcf-lobbies.sh start|stop|restart     every lobby
#   ./fcf-lobbies.sh status                      each lobby: running or not, its port, players lately
#   ./fcf-lobbies.sh logs N                      lobby N's log, following (Ctrl+C to stop)
#   sudo ./fcf-lobbies.sh remove                 stop and take the lobbies off (keeps /opt/fogcityfury)
#
# Settings (environment, for install): FCF_NAME (default THADAV.COM: lobbies are named "THADAV.COM 1" ...),
# FCF_BASEPORT (27015), FCF_EXTRA (more server flags for every lobby, e.g. "--laps 4 --weather random").
# Event files every lobby sends its players: put them in /opt/fogcityfury/events (trackN.fcfe), then restart.
# The ports must reach this machine: UDP 27015-27022 forwarded on the router to it. The script opens them in ufw or
# firewalld when either is running.
set -e
DIR=/opt/fogcityfury
UNIT=fogcityfury-lobby@.service
NAME=${FCF_NAME:-THADAV.COM}
BASE=${FCF_BASEPORT:-27015}
EXTRA=${FCF_EXTRA:-}
HERE=$(cd "$(dirname "$0")" && pwd)

lobbies() { ls "$DIR" 2>/dev/null | sed -n 's/^lobby\([0-9][0-9]*\)\.env$/\1/p' | sort -n; }
need_root() { [ "$(id -u)" = 0 ] || { echo "run it with sudo: sudo $0 $*"; exit 1; }; }
need_systemd() { command -v systemctl >/dev/null 2>&1 || { echo "this needs systemd (systemctl); most Linux systems have it"; exit 1; }; }
find_server() {
    for s in "$1" ./fogcityfury-server "$HERE/fogcityfury-server" "$HERE/../fogcityfury-server"; do
        [ -n "$s" ] && [ -f "$s" ] && { echo "$s"; return; }
    done
    echo "no fogcityfury-server found: give its path (from the release, or 'make fogcityfury-server')" >&2; exit 1
}
ports_open() {   # $1 first port, $2 last
    if command -v ufw >/dev/null 2>&1 && ufw status 2>/dev/null | grep -q "Status: active"; then ufw allow "$1:$2/udp" >/dev/null && echo "ufw: UDP $1-$2 open"
    elif command -v firewall-cmd >/dev/null 2>&1 && firewall-cmd --state >/dev/null 2>&1; then
        firewall-cmd --permanent --add-port="$1-$2/udp" >/dev/null && firewall-cmd --reload >/dev/null && echo "firewalld: UDP $1-$2 open"
    else echo "no firewall running here to open; make sure UDP $1-$2 reaches this machine (router port forwarding)"; fi
}

cmd=${1:-help}; [ $# -gt 0 ] && shift
case "$cmd" in
install)
    need_root "$cmd"; need_systemd
    N=${1:-8}; case "$N" in ''|*[!0-9]*) echo "N: a number of lobbies"; exit 1;; esac
    [ "$N" -ge 1 ] && [ "$N" -le 32 ] || { echo "N: 1 to 32"; exit 1; }
    SRV=$(find_server "$2")
    id fcf >/dev/null 2>&1 || useradd --system --home "$DIR" --shell /usr/sbin/nologin fcf
    mkdir -p "$DIR/events"
    install -m 755 "$SRV" "$DIR/fogcityfury-server"
    [ -f "$HERE/$UNIT" ] || { echo "$UNIT is not beside this script"; exit 1; }
    install -m 644 "$HERE/$UNIT" "/etc/systemd/system/$UNIT"
    # the old single server (fogcityfury-server.service) used port 27015: lobby 1 takes it over
    if systemctl list-unit-files fogcityfury-server.service >/dev/null 2>&1 && systemctl is-enabled fogcityfury-server >/dev/null 2>&1; then
        systemctl disable --now fogcityfury-server && echo "the old single server is off: lobby 1 has its port"
        [ -d "$DIR/tracks" ] && [ ! -d "$DIR/lobby1/tracks" ] && mkdir -p "$DIR/lobby1" && cp -r "$DIR/tracks" "$DIR/lobby1/tracks"
    fi
    for i in $(lobbies); do [ "$i" -gt "$N" ] && { systemctl disable --now "fogcityfury-lobby@$i" 2>/dev/null || true; rm -f "$DIR/lobby$i.env"; echo "lobby $i off"; }; done
    i=1
    while [ "$i" -le "$N" ]; do
        mkdir -p "$DIR/lobby$i/tracks"
        printf 'PORT=%s\nNAME="%s %s"\nEXTRA=%s\n' "$((BASE + i - 1))" "$NAME" "$i" "$EXTRA" > "$DIR/lobby$i.env"
        i=$((i + 1))
    done
    chown -R fcf:fcf "$DIR"
    systemctl daemon-reload
    for i in $(lobbies); do systemctl enable "fogcityfury-lobby@$i" >/dev/null 2>&1; systemctl restart "fogcityfury-lobby@$i"; done
    ports_open "$BASE" "$((BASE + N - 1))"
    sleep 2; "$0" status
    ;;
update)
    need_root "$cmd"; need_systemd; SRV=$(find_server "$1")
    install -m 755 "$SRV" "$DIR/fogcityfury-server" && chown fcf:fcf "$DIR/fogcityfury-server"
    for i in $(lobbies); do systemctl restart "fogcityfury-lobby@$i"; done; echo "every lobby restarted on the new server"
    ;;
start|stop|restart)
    need_root "$cmd"; need_systemd
    for i in $(lobbies); do systemctl "$cmd" "fogcityfury-lobby@$i"; done; [ "$cmd" = stop ] || { sleep 2; "$0" status; }
    ;;
status)
    need_systemd; [ -n "$(lobbies)" ] || { echo "no lobbies set up (sudo $0 install)"; exit 0; }
    for i in $(lobbies); do
        p=$(sed -n 's/^PORT=//p' "$DIR/lobby$i.env"); s=$(systemctl is-active "fogcityfury-lobby@$i" 2>/dev/null || true)
        j=$(journalctl -u "fogcityfury-lobby@$i" --since "-60min" --no-pager -q 2>/dev/null | grep -ci "join" || true)
        printf 'lobby %-2s  UDP %-5s  %-8s  %s joins in the last hour\n' "$i" "$p" "$s" "$j"
    done
    ;;
logs)
    need_systemd; journalctl -u "fogcityfury-lobby@${1:-1}" -f
    ;;
remove)
    need_root "$cmd"; need_systemd
    for i in $(lobbies); do systemctl disable --now "fogcityfury-lobby@$i" 2>/dev/null || true; rm -f "$DIR/lobby$i.env"; done
    rm -f "/etc/systemd/system/$UNIT"; systemctl daemon-reload; echo "lobbies removed ($DIR kept: its tracks and events)"
    ;;
*)
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    ;;
esac

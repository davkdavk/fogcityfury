# Fog City Fury in a web page (3.0)

The browser build is the same game, the same C++, compiled to WebAssembly with Emscripten. It draws with WebGL2
(the OpenGL ES 3.0 path Android uses). Nothing is downloaded but the game itself (about 4 MB): tracks, sound and
music are made in the game, as on every other platform.

3.0 comes in four steps: **single player in the browser** (2.9.9), **the lobbies in the browser** (2.9.9.2, below),
track editing in the page, and track submission. See the roadmap.

## Build

    # once: the Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html)
    git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install latest && ./emsdk activate latest
    source ./emsdk_env.sh

    make web          # web/fcf.js and web/fcf.wasm, beside web/index.html

Check it locally with any web server (a page can't load WebAssembly from a file:// address):

    cd web && python3 -m http.server 8000      # then open http://localhost:8000

## Put it on the site (Apache)

Copy `web/index.html`, `web/fcf.js` and `web/fcf.wasm` to a folder on the site, for example `/var/www/html/fcf/play/`
for `https://thadav.com/fcf/play/`. Apache must serve `.wasm` as WebAssembly, and compressing it cuts the 4 MB
download to about 1.5 MB. In the site's config, or a `.htaccess` in that folder:

    AddType application/wasm .wasm
    <IfModule mod_deflate.c>
      AddOutputFilterByType DEFLATE application/wasm application/javascript text/html
    </IfModule>

Then link to it from the game's page.

## Online in the browser: the lobbies (2.9.9.2)

A web page cannot send the UDP the lobby servers use, so browser players come in over a WebSocket and a small
program beside the lobbies, `fogcityfury-wsbridge`, passes each packet to the lobby's UDP port and back:

    browser  --wss://thadav.com/fcf/lobby/27019-->  Apache  --ws://LOBBY-MACHINE:27080/lobby/27019-->  wsbridge  --UDP-->  lobby 5

The lobby servers are unchanged; to them a browser player is a player like any other (from the bridge's machine).
Desktop, Android and 360 players keep coming straight in over UDP, and everyone races together.

1. **Run the bridge with the lobbies.** Put `FogCityFury-WSBridge-ARM64` (from the release, or `make
   fogcityfury-wsbridge-arm64`) in the folder with the lobby server and `lobbies.sh`, then `./lobbies.sh restart`:
   it starts the bridge too, on TCP 27080. `./lobbies.sh status` shows it. Nothing to forward on the router for it.
2. **Let Apache pass the lobby WebSockets to it.** Once:

        sudo a2enmod proxy proxy_wstunnel

   and in the HTTPS site (the `<VirtualHost *:443>` for thadav.com), add:

        ProxyPass "/fcf/lobby/" "ws://127.0.0.1:27080/lobby/"

   If the lobbies run on another machine than Apache, put that machine's address in place of 127.0.0.1. Then
   `sudo systemctl reload apache2`.

That is all: the web game's online screen lists the lobbies and joins them. (A page on another site can point the
game elsewhere with `Module.fcfLobbyUrl = 'wss://{host}/path/{port}'` in its index.html.)

## How it differs from the other builds

- **The main loop** runs as it does everywhere; once a frame it hands control back to the page (Emscripten's
  ASYNCIFY), paced to 60 frames a second.
- **Saves, settings, your tracks and their events** are kept in the browser's own storage for the site (IndexedDB),
  written every few seconds and when the tab is hidden. Clearing the site's data clears them.
- **The window** is the browser window; the game follows its size. Phones: hold them landscape.
- **Esc on the title** does nothing (there is nothing to quit to); close the tab instead.
- **Online** goes over WebSockets through the site (above); the online screen waits up to 4 s for a lobby's answer.
- **WebGL** refuses to draw from a texture while drawing into it (desktop OpenGL lets it through); the web build's GL
  layer unbinds such a texture for that pass (`gfx_gl.h`).

Players need a browser with WebGL2: a current Chrome, Firefox or Edge, or Safari 15 or later.

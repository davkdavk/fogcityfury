# Fog City Fury in a web page (3.0)

The browser build is the same game, the same C++, compiled to WebAssembly with Emscripten. It draws with WebGL2
(the OpenGL ES 3.0 path Android uses). Nothing is downloaded but the game itself (about 4 MB): tracks, sound and
music are made in the game, as on every other platform.

3.0 comes in four steps. **Step 1, this one: single player in the browser.** Then the lobbies (five 10-player
lobbies hosted on the site), track editing in the page, and track submission. See the roadmap.

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

## How it differs from the other builds

- **The main loop** runs as it does everywhere; once a frame it hands control back to the page (Emscripten's
  ASYNCIFY), paced to 60 frames a second.
- **Saves, settings, your tracks and their events** are kept in the browser's own storage for the site (IndexedDB),
  written every few seconds and when the tab is hidden. Clearing the site's data clears them.
- **The window** is the browser window; the game follows its size. Phones: hold them landscape.
- **Esc on the title** does nothing (there is nothing to quit to); close the tab instead.
- **Online** needs step 2: browsers cannot send the UDP the servers use, so each lobby server gets a WebSocket side.
- **WebGL** refuses to draw from a texture while drawing into it (desktop OpenGL lets it through); the web build's GL
  layer unbinds such a texture for that pass (`gfx_gl.h`).

Players need a browser with WebGL2: a current Chrome, Firefox or Edge, or Safari 15 or later.

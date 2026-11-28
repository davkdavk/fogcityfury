// radio.h - 2.9.9.6 internet radio in the races (an MP3 Shoutcast / Icecast stream), over the game's own music
//
// The station: music/radio.txt (an address, or a playlist pasted in), else music/radio.pls or radio.m3u (the station's
// playlist file, saved there); on the web, fetched from music/ beside the page. Or --radio URL. A stream address
// (http(s)://host:port/stream), or a .pls / .m3u playlist (an address, or the file), whose entries are tried in turn. M cycles the music: RADIO -> OWN (the built-in songs and your mp3s) -> OFF; the choice is saved. The radio
// plays in the races only: the menus keep main.mp3 (or the title song) and the showdown its own.
//
// PC, Windows, Android: a thread of its own reads the stream (http itself; https through curl, 2.9.9.7) into
// radio::ring (mp3music.h), where the audio thread decodes it. Lost: it tries again every few seconds, and the game's
// music comes back while it does. Web: the browser plays it (an <audio> element; an https stream, since the page is).
// Not on the 360.
#pragma once
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace radio {
static std::string url;   // the station (optRadio, settings.h: 0 own music, 1 the radio in races)
static std::string status = "";   // for the HUD and the tests: "", "CONNECTING", "PLAYING", "LOST", or why not
static bool on = false;           // what frame() last asked for
// the station from music/: radio.txt (an address, or a playlist pasted in), else radio.pls or radio.m3u (the station's own
// playlist file, saved there). A playlist is kept whole (url then holds it): its entries are tried in turn
static std::string firstHttp(const std::string& body) {
    size_t i = 0; while (i < body.size()) { size_t e = body.find('\n', i); if (e == std::string::npos) e = body.size(); std::string l = body.substr(i, e - i); i = e + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back(); size_t h = l.find("http"); if (h != std::string::npos && (h == 0 || l.compare(0, 4, "File") == 0)) return l.substr(h); }
    return "";
}
static void takeStation(std::string b) {
    while (!b.empty() && (b.back() == '\n' || b.back() == '\r' || b.back() == ' ')) b.pop_back();
    bool list = b.compare(0, 10, "[playlist]") == 0 || b.compare(0, 7, "#EXTM3U") == 0 || b.find("File1=") != std::string::npos;
    if (!list) { size_t e = b.find_first_of("\r\n"); if (e != std::string::npos) b.resize(e); while (!b.empty() && b.back() == ' ') b.pop_back(); }
    if (!b.empty() && url.empty()) url = b;
}
static bool isList(const std::string& u) { return u.compare(0, 4, "http") != 0; }

#if defined(__EMSCRIPTEN__)
EM_JS(void, fcfRadioPlay, (const char* u), { var s = UTF8ToString(u); if (!Module.fcfRadio) { Module.fcfRadio = new Audio(); Module.fcfRadio.preload = 'none'; }
    var a = Module.fcfRadio; if (a.getAttribute('src') !== s) a.src = s; if (a.paused) { var p = a.play(); if (p && p.catch) p.catch(function (e) { }); } });
EM_JS(void, fcfRadioStop, (), { var a = Module.fcfRadio; if (a) { a.pause(); a.removeAttribute('src'); a.load(); } });
EM_JS(void, fcfRadioVol, (float v), { var a = Module.fcfRadio; if (a) a.volume = Math.max(0, Math.min(1, v)); });
EM_JS(int, fcfRadioLive, (), { var a = Module.fcfRadio; return a && !a.paused && a.readyState >= 3 ? 1 : 0; });
static int tryNo = 0; static const char* TRY[3] = { "music/radio.txt", "music/radio.pls", "music/radio.m3u" };
static void urlErr(const char*);
static void urlOk(const char* file) { std::string b; FILE* f = fopen(file, "rb"); if (f) { char c[4096]; size_t k = fread(c, 1, sizeof c - 1, f); c[k] = 0; b = c; fclose(f); }
    takeStation(b); if (url.empty()) urlErr(file); }
static void urlErr(const char*) { if (++tryNo < 3) emscripten_async_wget(TRY[tryNo], "/radio.txt", urlOk, urlErr); }
static void init() { tryNo = 0; emscripten_async_wget(TRY[0], "/radio.txt", urlOk, urlErr); }
static void start() { std::string u = isList(url) ? firstHttp(url) : url; status = "PLAYING"; fcfRadioPlay(u.c_str()); }   // (the browser plays a stream, not a playlist: its first entry)
static void stop() { fcfRadioStop(); status = ""; }
#elif !defined(FCF_GFX_XSDK)
static std::atomic<int> gen{ 0 }; static std::atomic<int> state{ 0 };   // state: 0 idle, 1 connecting, 2 playing, 3 lost, 4 https and curl failed
struct Job { std::string url; int gen; };
static bool split(const std::string& u, std::string& host, int& port, std::string& path) {
    std::string r = u; if (r.compare(0, 7, "http://") != 0) return false; r = r.substr(7);
    size_t sl = r.find('/'); std::string hp = sl == std::string::npos ? r : r.substr(0, sl); path = sl == std::string::npos ? "/" : r.substr(sl);
    size_t c = hp.rfind(':'); port = 80; host = hp; if (c != std::string::npos) { host = hp.substr(0, c); port = atoi(hp.c_str() + c + 1); }
    return !host.empty() && port > 0 && port < 65536;
}
static int dial(const std::string& host, int port) {
    net::init(); addrinfo hints; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; addrinfo* res = nullptr;
    char ps[16]; snprintf(ps, sizeof ps, "%d", port); if (getaddrinfo(host.c_str(), ps, &hints, &res) != 0 || !res) return -1;
    int s = (int)socket(AF_INET, SOCK_STREAM, 0); if (s < 0) { freeaddrinfo(res); return -1; }
    bool ok = connect(s, res->ai_addr, (int)res->ai_addrlen) == 0; freeaddrinfo(res); if (!ok) { net::closeSock(s); return -1; } return s;
}
static int recvT(int s, char* b, int k, int ms) {   // -1 closed or broken, 0 nothing yet
    fd_set fs; FD_ZERO(&fs); FD_SET(s, &fs); timeval tv; tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    int r = select(s + 1, &fs, nullptr, nullptr, &tv); if (r < 0) return -1; if (r == 0) return 0;
    int got = (int)recv(s, b, k, 0); return got > 0 ? got : -1;
}
static std::vector<std::string> urlsIn(const std::string& body) {   // (the playlist's entries in order)   // a .pls ("File1=http://...") or .m3u (lines "http://...")
    std::vector<std::string> out; size_t i = 0;
    while (i < body.size()) { size_t e = body.find('\n', i); if (e == std::string::npos) e = body.size(); std::string l = body.substr(i, e - i); i = e + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back(); size_t h = l.find("http"); if (h != std::string::npos && (h == 0 || l.compare(0, 4, "File") == 0)) out.push_back(l.substr(h)); }
    return out;
}
static bool played = false;   // (the stream got as far as playing: a playlist's other entries are not needed)
static void stream(const std::string& u, int me, int depth);
static void playList(const std::string& body, int me, int depth) {   // each entry in turn, until one plays
    std::vector<std::string> us = urlsIn(body); if (us.empty()) { state = 3; return; }
    for (auto& v : us) { if (gen != me) return; played = false; stream(v, me, depth + 1); if (played) return; }
}
#ifdef _WIN32
// (not _popen: from a window app that starts cmd.exe, and its console flashes up. curl itself, hidden, its output a pipe)
static HANDLE curlProc = nullptr;
static FILE* FCF_POPEN(const char* cmd) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, nullptr, TRUE }; HANDLE rd = nullptr, wr = nullptr; if (!CreatePipe(&rd, &wr, &sa, 1 << 16)) return nullptr;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wr; si.hStdError = GetStdHandle(STD_ERROR_HANDLE); si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi; std::string c = cmd; size_t q = c.find(" 2>NUL"); if (q != std::string::npos) c.resize(q);
    if (!CreateProcessA(nullptr, &c[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(rd); CloseHandle(wr); return nullptr; }
    CloseHandle(wr); CloseHandle(pi.hThread); curlProc = pi.hProcess;
    int fd = _open_osfhandle((intptr_t)rd, _O_RDONLY | _O_BINARY); if (fd < 0) { CloseHandle(rd); return nullptr; } return _fdopen(fd, "rb");
}
static void FCF_PCLOSE(FILE* f) { if (curlProc) { TerminateProcess(curlProc, 0); CloseHandle(curlProc); curlProc = nullptr; } fclose(f); }
#else
#define FCF_POPEN(c) popen(c, "r")
#define FCF_PCLOSE pclose
#endif
// 2.9.9.7 an https station: curl reads it (Windows 10 and later, macOS and most Linux have it), into the same ring. It
// follows redirects; a playlist it fetches is read like any other. Closing the pipe when the radio stops ends it
static void streamCurl(const std::string& u, int me, int depth) {
    for (char c : u) if (strchr("\"'`$;&|<>\\ \t\r\n(){}*?!", c)) { state = 3; return; }   // (radio.txt goes on a command line: nothing a shell reads)
#ifdef _WIN32
    std::string cmd = "curl.exe -sSL --fail -A FogCityFury/2.9.9.7 --connect-timeout 8 \"" + u + "\" 2>NUL";
#else
    std::string cmd = "curl -sSL --fail -A FogCityFury/2.9.9.7 --connect-timeout 8 '" + u + "' 2>/dev/null";
#endif
    state = 1; FILE* p = FCF_POPEN(cmd.c_str()); if (!p) { state = 4; return; }
    char b[8192]; std::string first; size_t k = fread(b, 1, 2048, p); if (k == 0) { FCF_PCLOSE(p); state = 4; return; }
    first.assign(b, k);
    bool list = first.compare(0, 10, "[playlist]") == 0 || first.compare(0, 7, "#EXTM3U") == 0 || first.find("File1=") != std::string::npos || (first.compare(0, 4, "http") == 0 && first.size() < 2048);
    if (list && depth < 4) { while ((k = fread(b, 1, sizeof b, p)) > 0 && first.size() < 65536) first.append(b, k); FCF_PCLOSE(p); playList(first, me, depth); return; }
    reset = true; state = 2; played = true; put((const uint8_t*)first.data(), first.size());
    while (gen == me && (k = fread(b, 1, 4096, p)) > 0) put((const uint8_t*)b, k);
    FCF_PCLOSE(p); if (gen == me) state = 3;
}
static void stream(const std::string& u, int me, int depth) {   // until the station is lost or the radio is turned off
    if (u.compare(0, 8, "https://") == 0 || getenv("FCF_CURL")) { streamCurl(u, me, depth); return; }   // (FCF_CURL: the tests, curl on a plain http station)
    std::string host, path; int port; if (!split(u, host, port, path)) { state = 3; return; }
    state = 1; int s = dial(host, port); if (s < 0) { state = 3; return; }
    std::string req = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nUser-Agent: FogCityFury/2.9.9.6\r\nAccept: */*\r\nIcy-MetaData: 0\r\nConnection: close\r\n\r\n";
    send(s, req.data(), (int)req.size(), 0);
    std::string head; char b[8192]; size_t endH = std::string::npos;
    for (int waited = 0; endH == std::string::npos && waited < 8000 && gen == me;) { int k = recvT(s, b, sizeof b, 250); if (k < 0) break; if (k == 0) { waited += 250; continue; }
        head.append(b, k); endH = head.find("\r\n\r\n"); if (endH == std::string::npos) endH = head.find("\n\n") != std::string::npos ? head.find("\n\n") : std::string::npos; if (head.size() > 65536) break; }
    if (endH == std::string::npos) { net::closeSock(s); state = 3; return; }
    std::string h = head.substr(0, endH), lower = h; for (char& c : lower) c = (char)tolower((unsigned char)c);
    std::string body = head.substr(endH + (head.compare(endH, 4, "\r\n\r\n") == 0 ? 4 : 2));
    int code = 0; { size_t sp = h.find(' '); if (sp != std::string::npos) code = atoi(h.c_str() + sp + 1); }   // (HTTP/1.x 200 or ICY 200)
    if (code >= 300 && code < 400 && depth < 4) {   // moved: follow it
        size_t l = lower.find("\nlocation:"); net::closeSock(s); if (l == std::string::npos) { state = 3; return; }
        std::string to = h.substr(l + 10); size_t e = to.find_first_of("\r\n"); if (e != std::string::npos) to.resize(e); while (!to.empty() && to[0] == ' ') to.erase(0, 1);
        stream(to, me, depth + 1); return;
    }
    if (code != 200) { net::closeSock(s); state = 3; return; }
    bool list = lower.find("scpls") != std::string::npos || lower.find("mpegurl") != std::string::npos || body.compare(0, 10, "[playlist]") == 0 || body.compare(0, 7, "#EXTM3U") == 0;
    if (list && depth < 4) {   // a playlist: read it all, then its first stream
        for (int k; gen == me && (k = recvT(s, b, sizeof b, 2000)) > 0 && body.size() < 65536;) body.append(b, k);
        net::closeSock(s); playList(body, me, depth); return;
    }
    reset = true; state = 2; played = true; put((const uint8_t*)body.data(), body.size());
    for (int quiet = 0; gen == me && quiet < 10000;) { int k = recvT(s, b, sizeof b, 500); if (k < 0) break; if (k == 0) { quiet += 500; continue; } quiet = 0; put((const uint8_t*)b, (size_t)k); }
    net::closeSock(s); if (gen == me) state = 3;
}
static int fetch(void* p) {
    Job* j = (Job*)p; std::string u = j->url; int me = j->gen; delete j;
    while (gen == me) { if (isList(u)) playList(u, me, 0); else stream(u, me, 0); for (int w = 0; w < 30 && gen == me; w++) net::sleepMs(100); }   // (lost: again in 3 s)
    return 0;
}
static void init() {
    for (const char* n : { "music/radio.txt", "music/radio.pls", "music/radio.m3u" }) { if (!url.empty()) return;
        FILE* f = fopen(n, "rb"); if (!f) continue; char c[4096]; size_t k = fread(c, 1, sizeof c - 1, f); c[k] = 0; fclose(f); takeStation(c); }
}
static void start() { int me = ++gen; plat::runDetached(fetch, new Job{ url, me }, "radio"); }
static void stop() { ++gen; state = 0; }
#else
static void init() {} static void start() { status = "NOT ON THE 360"; } static void stop() {}
#endif

// the game thread, every frame: is the radio wanted now (a race, the radio chosen, a station known)?
static void frame(bool race) {
    bool w = race && optRadio == 1 && !url.empty() && snd.musicOn;
    if (w != on) { on = w; want = w; if (w) start(); else stop(); }
#if defined(__EMSCRIPTEN__)
    if (on) { webLive = fcfRadioLive() != 0; fcfRadioVol(g * snd.music * snd.musicDuck * snd.master * 1.6f); }   // (g: the cross-fade, from the mixer)
    else webLive = false;
#elif !defined(FCF_GFX_XSDK)
    static const char* ST[5] = { "", "CONNECTING", "PLAYING", "LOST: TRYING AGAIN", "HTTPS: CURL COULD NOT REACH IT" }; status = on ? ST[std::max(0, std::min(4, state.load()))] : "";
#endif
}
}  // namespace radio

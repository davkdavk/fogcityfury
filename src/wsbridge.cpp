// Fog City Fury. Copyright (C) 2026 Dave (davkdavk) and contributors. GPL-3.0-or-later (see LICENSE).
// fogcityfury-wsbridge - lets players in a web browser into the lobbies. A web page cannot send UDP, so the browser
// game opens a WebSocket to wss://SITE/fcf/lobby/PORT; the site's web server (Apache, mod_proxy_wstunnel) passes it
// here as ws://THIS-MACHINE:27080/lobby/PORT, and this passes each message, a game packet, to the lobby's UDP port and
// each packet back, unchanged. Every browser player gets their own UDP socket, so to the lobby server each one is a
// player like any other (from this machine's address).
//
//   fogcityfury-wsbridge [--listen ADDR:PORT] [--ports FIRST-LAST] [--target HOST] [--max N] [--quiet]
//     --listen   where the web server connects (default 0.0.0.0:27080; the router does not forward it)
//     --ports    the lobby ports a browser may reach (default 27015-27022)
//     --target   where the lobbies are (default 127.0.0.1)
//     --max      browser players at once (default 200)
//
// One file, no libraries: plain sockets and poll(), its own SHA-1 for the handshake. Linux and other POSIX systems.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <string>
#include <vector>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static bool quiet = false;
// a log line with the time: f takes a string, then an int
static void say(const char* f, const char* a = "", int b = 0) { if (quiet) return; time_t t = time(nullptr); char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t)); printf("%s ", ts); printf(f, a, b); printf("\n"); fflush(stdout); }

// ---- SHA-1 and base64, for Sec-WebSocket-Accept ----
static void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::vector<uint8_t> m(data, data + len); m.push_back(0x80); while (m.size() % 64 != 56) m.push_back(0);
    uint64_t bits = (uint64_t)len * 8; for (int i = 7; i >= 0; i--) m.push_back((uint8_t)(bits >> (i * 8)));
    auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    for (size_t c = 0; c < m.size(); c += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[c + 4 * i] << 24 | (uint32_t)m[c + 4 * i + 1] << 16 | (uint32_t)m[c + 4 * i + 2] << 8 | m[c + 4 * i + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], cc = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; } else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i]; e = d; d = cc; cc = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += cc; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(h[i] >> (24 - 8 * j));
}
static std::string base64(const uint8_t* p, size_t n) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string o;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        o += T[v >> 18 & 63]; o += T[v >> 12 & 63]; o += i + 1 < n ? T[v >> 6 & 63] : '='; o += i + 2 < n ? T[v & 63] : '=';
    }
    return o;
}

// ---- one browser player ----
struct Client {
    int tcp = -1, udp = -1, port = 0; bool open = false, closing = false; time_t last = 0; char who[64] = "";
    std::string in, out;   // bytes from the browser not yet a whole frame; bytes to the browser not yet sent
};
static std::vector<Client> clients;
static int firstPort = 27015, lastPort = 27022, maxClients = 200; static std::string target = "127.0.0.1";

static void nonblock(int s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
static void frame(Client& c, int op, const uint8_t* p, size_t n) {   // server to browser: one unmasked frame
    if (c.out.size() > 512 * 1024) { c.closing = true; return; }   // (a browser that stopped reading: let it go)
    uint8_t h[10]; size_t hl = 2; h[0] = (uint8_t)(0x80 | op);
    if (n < 126) h[1] = (uint8_t)n; else if (n < 65536) { h[1] = 126; h[2] = (uint8_t)(n >> 8); h[3] = (uint8_t)n; hl = 4; }
    else { h[1] = 127; for (int i = 0; i < 8; i++) h[2 + i] = (uint8_t)((uint64_t)n >> (56 - 8 * i)); hl = 10; }
    c.out.append((const char*)h, hl); c.out.append((const char*)p, n);
}
static void drop(Client& c, const char* why) {
    if (c.tcp >= 0) close(c.tcp);
    if (c.udp >= 0) close(c.udp);
    if (c.open) { (void)why; say("browser %s disconnected", c.who); }
    c.tcp = c.udp = -1;
}
static bool handshake(Client& c) {   // true: done (or failed: c.closing); false: wait for more
    size_t end = c.in.find("\r\n\r\n"); if (end == std::string::npos) { if (c.in.size() > 8192) c.closing = true; return false; }
    std::string req = c.in.substr(0, end); c.in.erase(0, end + 4);
    auto header = [&](const char* name) -> std::string {
        std::string low = req; for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        std::string key = std::string("\n") + name + ":"; size_t i = low.find(key); if (i == std::string::npos) return "";
        i += key.size(); size_t e = req.find("\r\n", i); std::string v = req.substr(i, e == std::string::npos ? std::string::npos : e - i);
        while (!v.empty() && v[0] == ' ') v.erase(0, 1);
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        return v;
    };
    // the path's last number is the lobby's port: /lobby/27019 (or /fcf/lobby/27019 if the web server passes it whole)
    int port = 0; if (req.compare(0, 4, "GET ") == 0) { size_t sp = req.find(' ', 4); std::string path = req.substr(4, sp - 4);
        size_t q = path.find('?'); if (q != std::string::npos) path.erase(q); while (!path.empty() && path.back() == '/') path.pop_back();
        size_t sl = path.rfind('/'); if (sl != std::string::npos) port = atoi(path.c_str() + sl + 1); }
    std::string key = header("sec-websocket-key");
    if (key.empty() || port < firstPort || port > lastPort) {
        const char* r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; c.out = r; c.closing = true; return true;
    }
    std::string fwd = header("x-forwarded-for"); if (!fwd.empty()) { size_t cm = fwd.find(','); snprintf(c.who, sizeof c.who, "%s", fwd.substr(0, cm).c_str()); }
    c.udp = socket(AF_INET, SOCK_DGRAM, 0); sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    if (c.udp < 0 || inet_pton(AF_INET, target.c_str(), &a.sin_addr) != 1 || connect(c.udp, (sockaddr*)&a, sizeof a) != 0) { c.closing = true; return true; }
    nonblock(c.udp); c.port = port;
    std::string acc = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; uint8_t dg[20]; sha1((const uint8_t*)acc.data(), acc.size(), dg);
    c.out = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + base64(dg, 20) + "\r\n\r\n";
    c.open = true; say("browser %s connected to lobby port %d", c.who, port); return true;
}
static void frames(Client& c) {   // browser to server: whole frames out of c.in
    while (!c.closing) {
        if (c.in.size() < 2) return;
        const uint8_t* p = (const uint8_t*)c.in.data(); int op = p[0] & 15; bool masked = p[1] & 0x80;
        uint64_t n = p[1] & 127; size_t hl = 2;
        if (n == 126) { if (c.in.size() < 4) return; n = (uint64_t)p[2] << 8 | p[3]; hl = 4; }
        else if (n == 127) { if (c.in.size() < 10) return; n = 0; for (int i = 0; i < 8; i++) n = n << 8 | p[2 + i]; hl = 10; }
        if (!masked || n > 8192) { c.closing = true; return; }   // (browsers always mask; game packets are small)
        if (c.in.size() < hl + 4 + n) return;
        uint8_t mk[4]; memcpy(mk, p + hl, 4); std::string pl = c.in.substr(hl + 4, (size_t)n); c.in.erase(0, hl + 4 + (size_t)n);
        for (size_t i = 0; i < pl.size(); i++) pl[i] = (char)(pl[i] ^ mk[i & 3]);
        if (op == 2 || op == 0) { if (!pl.empty()) send(c.udp, pl.data(), pl.size(), 0); }   // a game packet: on to the lobby
        else if (op == 8) { frame(c, 8, nullptr, 0); c.closing = true; }                     // close
        else if (op == 9) frame(c, 10, (const uint8_t*)pl.data(), pl.size());               // ping: pong
        c.last = time(nullptr);
    }
}

int main(int argc, char** argv) {
    std::string listenAt = "0.0.0.0:27080";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i]; auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--listen") listenAt = next(); else if (a == "--target") target = next(); else if (a == "--max") maxClients = atoi(next().c_str());
        else if (a == "--ports") { std::string r = next(); size_t d = r.find('-'); firstPort = atoi(r.c_str()); lastPort = d == std::string::npos ? firstPort : atoi(r.c_str() + d + 1); }
        else if (a == "--quiet") quiet = true;
        else { printf("fogcityfury-wsbridge [--listen ADDR:PORT] [--ports FIRST-LAST] [--target HOST] [--max N] [--quiet]\n"); return a == "--help" ? 0 : 1; }
    }
    signal(SIGPIPE, SIG_IGN);
    size_t colon = listenAt.rfind(':'); std::string lhost = listenAt.substr(0, colon); int lport = atoi(listenAt.c_str() + colon + 1);
    int ls = socket(AF_INET, SOCK_STREAM, 0); int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in la; memset(&la, 0, sizeof la); la.sin_family = AF_INET; la.sin_port = htons((uint16_t)lport);
    if (inet_pton(AF_INET, lhost.c_str(), &la.sin_addr) != 1 || bind(ls, (sockaddr*)&la, sizeof la) != 0 || listen(ls, 64) != 0) {
        printf("fogcityfury-wsbridge: cannot listen on %s (%s)\n", listenAt.c_str(), strerror(errno)); return 1;
    }
    nonblock(ls);
    printf("fogcityfury-wsbridge: browser players on ws://%s/lobby/PORT -> UDP %s:%d-%d (up to %d)\n", listenAt.c_str(), target.c_str(), firstPort, lastPort, maxClients); fflush(stdout);
    std::vector<pollfd> pf; uint8_t buf[4096];
    for (;;) {
        pf.clear(); pf.push_back({ ls, POLLIN, 0 });
        for (auto& c : clients) { pf.push_back({ c.tcp, (short)(POLLIN | (c.out.empty() ? 0 : POLLOUT)), 0 }); pf.push_back({ c.udp, (short)(c.udp >= 0 ? POLLIN : 0), 0 }); }
        if (poll(pf.data(), pf.size(), 1000) < 0 && errno != EINTR) break;
        time_t now = time(nullptr);
        if (pf[0].revents & POLLIN) for (;;) {   // new connections
            sockaddr_in ca; socklen_t cl = sizeof ca; int s = accept(ls, (sockaddr*)&ca, &cl); if (s < 0) break;
            if ((int)clients.size() >= maxClients) { close(s); continue; }
            nonblock(s); setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
            Client c; c.tcp = s; c.last = now; inet_ntop(AF_INET, &ca.sin_addr, c.who, sizeof c.who); clients.push_back(c);
        }
        for (size_t k = 0; k < clients.size(); k++) {
            Client& c = clients[k]; if (1 + 2 * k + 1 >= pf.size()) break; short tr = pf[1 + 2 * k].revents, ur = pf[2 + 2 * k].revents;
            if (tr & (POLLIN | POLLHUP | POLLERR)) {
                for (;;) { ssize_t n = recv(c.tcp, buf, sizeof buf, 0); if (n > 0) { c.in.append((const char*)buf, (size_t)n); if (c.in.size() > 64 * 1024) { c.closing = true; break; } continue; }
                    if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) { c.closing = true; c.out.clear(); } break; }
                if (!c.open && !c.closing) handshake(c);
                if (c.open) frames(c);
            }
            if (c.udp >= 0 && (ur & POLLIN)) for (;;) { ssize_t n = recv(c.udp, buf, sizeof buf, 0); if (n <= 0) break; frame(c, 2, buf, (size_t)n); c.last = now; }   // the lobby's packets back
            if (!c.out.empty()) { ssize_t n = send(c.tcp, c.out.data(), c.out.size(), MSG_NOSIGNAL); if (n > 0) c.out.erase(0, (size_t)n); else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { c.closing = true; c.out.clear(); } }
            if (now - c.last > 60) c.closing = true;   // (a minute with nothing either way)
        }
        for (size_t k = 0; k < clients.size();) { Client& c = clients[k]; if (c.closing && c.out.empty()) { drop(c, ""); clients.erase(clients.begin() + k); } else if (c.closing && now - c.last > 5) { drop(c, ""); clients.erase(clients.begin() + k); } else k++; }
    }
    return 0;
}

// serverlist.h - the server list on the online screen: lookups, asking each server what it runs, joining.
// Part of game.cpp, which includes it at this point: one translation unit, so this uses whatever is declared
// above its #include there, and what follows the #include can use what is declared here.
#pragma once
// ---- server list: the online screen shows these, asks each what it is running, and joins the one picked ----
struct DnsJob { std::string host; sockaddr_in addr; plat::Atomic done; };     // done: 0 looking up, 1 found, 2 not found
struct ServerEntry {
    std::string host; std::shared_ptr<DnsJob> job; sockaddr_in addr; int st = 0;   // st: 0 looking up the address, 1 asked, 2 answered, 3 no reply, 4 unknown address
    bool waiting = false; uint32_t nonce = 0; double sent = 0; float ping = 0; net::Info info; uint16_t proto = 0;
};
// the built-in list: the eight lobbies at thadav.com (deploy/fcf-lobbies.sh: 27015-27022), and a server on this computer
static const char* DEFAULT_SERVERS[] = { "thadav.com:27015", "thadav.com:27016", "thadav.com:27017", "thadav.com:27018", "thadav.com:27019",
    "thadav.com:27020", "thadav.com:27021", "thadav.com:27022", "127.0.0.1:27015" }; static const size_t MAX_SERVERS = 9;
static const int LOBBY_LIST = 1;   // (2.9.9.1: a saved list from before the eight lobbies gets the ones it lacks, once)
static std::vector<ServerEntry> servers; static int srvSel = 0, srvMode = 0, qsock = -1;   // srvMode: 0 picking, 1 typing a new address, 2 typing our name
static char srvNew[96] = ""; static float srvRefresh = 0; static bool srvSkipText = false;
// Typing an address or a name with only a controller (the Xbox 360): a grid of letters picked with the d-pad, A types the
// one picked. The last row: SPACE, DEL and DONE
static const char* OSK_ROWS[4] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.-:_", "0123456789" }; static int oskX = 0, oskY = 0;
static void srvType(const char* t) {   // add typed text to the address or name being typed
    char* f = srvMode == 2 ? nc.name : srvNew; size_t cap = srvMode == 2 ? net::NAMELEN : sizeof srvNew - 1;
    for (const char* c = t; *c; c++) { size_t n = strlen(f); char ch = srvMode == 2 ? (char)toupper((unsigned char)*c) : (char)tolower((unsigned char)*c); if (n < cap && ch >= 32 && ch < 127 && !(ch == ' ' && srvMode == 1)) { f[n] = ch; f[n + 1] = 0; } }
}
static int dnsThread(void* p) { std::shared_ptr<DnsJob>* sp = (std::shared_ptr<DnsJob>*)p; bool ok = net::resolve((*sp)->host.c_str(), (*sp)->addr); plat::atomicSet((*sp)->done, ok ? 1 : 2); delete sp; return 0; }
// look the address up on another thread: a slow or dead DNS server must not freeze the menu
static void srvLookup(ServerEntry& e) {
    net::init(); e.st = 0; e.waiting = false; e.job = std::make_shared<DnsJob>(); e.job->host = e.host; plat::atomicSet(e.job->done, 0);
    auto* sp = new std::shared_ptr<DnsJob>(e.job); plat::runDetached(dnsThread, sp, "dns");
}
static void srvAsk(ServerEntry& e) {
    if (qsock < 0) return; static uint32_t n = 1; e.nonce = (uint32_t)plat::ticks() * 2654435761u + n++;
    net::Query q; memset(&q, 0, sizeof q); q.h = net::header(net::C_QUERY, 0xFF); q.nonce = e.nonce; net::sendTo(qsock, e.addr, &q, sizeof q); e.sent = plat::nowSec(); e.waiting = true; if (e.st != 2) e.st = 1;
}
static void srvAdd(const char* host) { for (auto& e : servers) if (e.host == host) return; if (servers.size() >= MAX_SERVERS || !host[0]) return; ServerEntry e; e.host = host; e.st = 4; servers.push_back(e); }
static void srvOpen() { if (qsock < 0) qsock = net::openUdp(0); for (auto& e : servers) srvLookup(e); srvRefresh = 0; srvMode = 0; if (srvSel >= (int)servers.size()) srvSel = 0; }
static void srvClose() { if (qsock >= 0) net::closeSock(qsock); qsock = -1; }
static void srvPump(float dt) {
    if (qsock < 0) return; srvSkipText = false;
    for (auto& e : servers) {
        if (e.st == 0 && e.job) { int d = plat::atomicGet(e.job->done); if (d == 1) { e.addr = e.job->addr; srvAsk(e); } else if (d == 2) e.st = 4; }
        if (e.waiting && plat::nowSec() - e.sent > 1.5) {
#ifdef FCF_GFX_XSDK
            xsdk_log_write("net: server info timeout host=%s after 1500 ms", e.host.c_str());
#endif
            e.waiting = false; e.st = 3;
        }
    }
    uint8_t buf[256]; sockaddr_in from;
    for (int guard = 0; guard < 64; guard++) {
        int n = net::recvFrom(qsock, buf, sizeof buf, from); if (n <= 0) break; if (!net::valid(buf, n, sizeof(net::Info))) continue;
        net::Info in; memcpy(&in, buf, sizeof in); if (in.h.type != net::S_INFO) continue;
        for (auto& e : servers) if (e.waiting && e.nonce == in.nonce && net::sameAddr(e.addr, from)) { e.waiting = false; e.st = 2; e.ping = (float)((plat::nowSec() - e.sent) * 1000); e.info = in; e.info.name[sizeof e.info.name - 1] = 0; e.proto = in.h.proto; }
    }
    srvRefresh += dt; if (srvRefresh > 4) { srvRefresh = 0; for (auto& e : servers) if (e.st == 1 || e.st == 2 || e.st == 3) srvAsk(e); }   // keep the list fresh
}

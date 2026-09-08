//
//  NameBrute — multithreaded name (channel) brute force for the G HUB update
//  pipeline:  GET https://<host>/pipeline/<ver>/update/<app>/<platform>/<name>/<suffix>
//             (<ver> = v1 or v2, selected with -V)
//
//  Result classification per probe:
//    200 + valid JSON object  -> printed in GREEN  (name exists, plaintext manifest)
//    200 + non-JSON body      -> printed in BLUE   (name exists, opaque/encrypted body)
//    anything else (403/404…) -> not printed       (name unavailable)
//
//  Name sources (exactly one):
//    positional args:   NameBrute public canary tim
//    -f <file>:         one name per line, '#' comments and blank lines skipped
//    -s:                generated sequence a, b, … z, aa, ab, … zz, aaa, aab, …
//                       (odometer over -c charset, lengths 1..-L, -L 0 = unbounded)
//

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_KEEPALIVE_MAX_COUNT 100000 // never re-handshake mid-run
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>

#include "colors.hpp"
#include "httplib/httplib.h"
#include "jsoncpp/json/json.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    std::string host = "updates.ghub.logitechg.com";
    std::string apiVer = "v2"; //    v1 (older pipeline) or v2
    std::string app = "ghub10";
    std::string platform = "win";
    std::string suffix = "update.json";

    int threads = 0; //        0 -> hardware_concurrency
    int timeoutMs = 10000; //  per-request timeout
    int delayMs = 0;       //  per-request delay per worker (throttle)

    int shardI = -1; //        -1 = disabled; else probe names with
    int shardN = 1; //        (global index) % shardN == shardI

    bool h2Mode = false;      // -h2: one TLS connection per thread, up to
                              //     128 in-flight streams each (server cap)

    bool sequenceMode = false;
    std::string file;
    std::string charset = "abcdefghijklmnopqrstuvwxyz";
    int maxLen = 2; //        sequence max length, 0 = unbounded
    std::vector<std::string> args;
};

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

void usage() {
    std::cout <<
        "NameBrute — multithreaded channel-name brute force\n"
        "\n"
        "usage: NameBrute [options] [name1 name2 ...]\n"
        "\n"
        "name sources (exactly one):\n"
        "  name1 name2 ...   probe the given names\n"
        "  -f <file>         one name per line ('#' comments, blanks skipped)\n"
        "  -s                generate a, b, ... z, aa, ab, ... zz, aaa, aab, ...\n"
        "\n"
        "options:\n"
        "  -h2          HTTP/2 mode: each thread = 1 connection multiplexing\n"
        "               up to 128 in-flight streams (server-enforced cap).\n"
        "               Avoids the per-IP HTTP/1.1 connection cliff (~1024);\n"
        "               measured ~8k/s per IP with -t 32. -d is ignored.\n"
        "  -t <n>       worker threads (default: CPU count, max 16384; each\n"
        "               thread = 1 connection = 1 in-flight request,\n"
        "               or up to 128 in-flight streams with -h2)\n"
        "  -shard <i>/<n>  probe only names where (global index) % n == i-1.\n"
        "               Run N instances with i = 1..N to multiply concurrency\n"
        "               by N (each process has its own socket/handle budget)\n"
        "  -h <host>    default updates.ghub.logitechg.com\n"
        "  -V <v1|v2>   pipeline API version, default v2 (v1 = older apps\n"
        "               such as ghub7; both versions use the same layout)\n"
        "  -a <app>     app id, default ghub10\n"
        "  -p <plat>    platform, default win\n"
        "  -u <suffix>  endpoint suffix, default update.json (short update\n"
        "               check, ~200 B); use details.json for the full\n"
        "               manifest with depots (~100 kB per response)\n"
        "  -T <ms>      request timeout, default 10000\n"
        "  -d <ms>      per-request delay per thread, default 0\n"
        "  -c <chars>   sequence charset, default a-z\n"
        "  -L <len>     sequence max length, default 2 (0 = unbounded, Ctrl+C)\n"
        "  -?           this help\n"
        "\n"
        "output: found names are printed, plaintext in green, encrypted in\n"
        "blue, unavailable names are not printed. A timestamped .txt log is\n"
        "created next to the executable (parameters, finds, summary) and the\n"
        "path is printed on start. Ctrl+C stops the run.\n";
}

bool parseArgs(int argc, char** argv, Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << what << " needs a value\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "-?" || a == "--help") {
            usage();
            exit(0);
        } else if (a == "-h2") {
            cfg.h2Mode = true;
        } else if (a == "-t") {
            cfg.threads = std::stoi(need("-t"));
        } else if (a == "-V") {
            cfg.apiVer = need("-V");
            if (cfg.apiVer != "v1" && cfg.apiVer != "v2") {
                std::cerr << "error: -V must be v1 or v2\n";
                exit(2);
            }
        } else if (a == "-h") {
            cfg.host = need("-h");
        } else if (a == "-a") {
            cfg.app = need("-a");
        } else if (a == "-p") {
            cfg.platform = need("-p");
        } else if (a == "-u") {
            cfg.suffix = need("-u");
        } else if (a == "-T") {
            cfg.timeoutMs = std::stoi(need("-T"));
        } else if (a == "-d") {
            cfg.delayMs = std::stoi(need("-d"));
        } else if (a == "-c") {
            cfg.charset = need("-c");
        } else if (a == "-L") {
            cfg.maxLen = std::stoi(need("-L"));
        } else if (a == "-shard") {
            const std::string v = need("-shard");
            const size_t slash = v.find('/');
            if (slash == std::string::npos || slash + 1 >= v.size()) {
                std::cerr << "error: -shard needs <i>/<n> (e.g. 1/4)\n";
                exit(2);
            }
            const int i = std::stoi(v.substr(0, slash));
            const int n = std::stoi(v.substr(slash + 1));
            if (n < 1 || i < 1 || i > n) {
                std::cerr << "error: -shard needs 1 <= " << i << " <= " << n << " <= 65535\n";
                exit(2);
            }
            cfg.shardI = i - 1;
            cfg.shardN = n;
        } else if (a == "-f") {
            cfg.file = need("-f");
            if (cfg.sequenceMode) {
                std::cerr << "error: -f and -s are mutually exclusive\n";
                exit(2);
            }
        } else if (a == "-s") {
            cfg.sequenceMode = true;
            if (!cfg.file.empty()) {
                std::cerr << "error: -f and -s are mutually exclusive\n";
                exit(2);
            }
        } else if (!a.empty() && a[0] == '-' && a.size() > 1) {
            std::cerr << "error: unknown option " << a << "\n";
            usage();
            exit(2);
        } else {
            if (cfg.sequenceMode) {
                std::cerr << "error: -s takes no name arguments\n";
                exit(2);
            }
            cfg.args.push_back(trim(a));
        }
    }

    bool hasFile = !cfg.file.empty();
    bool hasArgs = !cfg.args.empty();
    if (cfg.sequenceMode && (hasFile || hasArgs)) {
        std::cerr << "error: -s takes no name arguments\n";
        exit(2);
    }
    if (hasFile + hasArgs + (int)cfg.sequenceMode != 1) {
        std::cerr << "error: give exactly one name source: names, -f <file>, or -s\n";
        usage();
        exit(2);
    }
    if (cfg.threads < 1) cfg.threads = 1;
    if (cfg.threads > 16384) cfg.threads = 16384;
    if (cfg.delayMs < 0) cfg.delayMs = 0;
    if (cfg.timeoutMs < 100) cfg.timeoutMs = 100;
    if (cfg.maxLen < 0) cfg.maxLen = 0;
    if (cfg.sequenceMode && cfg.charset.size() < 2) {
        std::cerr << "error: sequence charset needs at least 2 distinct characters\n";
        exit(2);
    }
    for (char ch : cfg.charset) {
        if (ch < 0x20 || ch > 0x7e) {
            std::cerr << "error: sequence charset must be printable ASCII\n";
            exit(2);
        }
    }
    return true;
}

std::vector<std::string> loadNamesFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot open " << path << "\n";
        exit(2);
    }
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        out.push_back(line);
    }
    if (out.empty()) {
        std::cerr << "error: no names in " << path << "\n";
        exit(2);
    }
    return out;
}

// Odometer: 1 -> "a", 2 -> "b", ... 26 -> "z", 27 -> "aa", 28 -> "ab", ...
std::string sequenceName(uint64_t index1, const std::string& cs) {
    uint64_t v = index1 - 1;
    std::string s;
    do {
        s.push_back(static_cast<char>(cs[v % cs.size()]));
        v /= static_cast<uint64_t>(cs.size());
    } while (v);
    std::reverse(s.begin(), s.end());
    return s;
}

// Total number of sequence names for lengths 1..maxLen; UINT64_MAX = unbounded.
uint64_t sequenceTotal(int maxLen, const std::string& cs) {
    if (maxLen == 0) return UINT64_MAX;
    uint64_t total = 0, power = 1;
    const uint64_t base = static_cast<uint64_t>(cs.size());
    for (int len = 1; len <= maxLen; ++len) {
        if (power > UINT64_MAX / base) return UINT64_MAX;
        power *= base;
        if (power > UINT64_MAX - total) return UINT64_MAX;
        total += power;
    }
    return total;
}

// Percent-encode one URL path segment (unreserved chars pass through).
std::string encodeSegment(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out.push_back(static_cast<char>(c));
        else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

bool isJsonObject(const std::string& body, Json::Value& root) {
    Json::CharReaderBuilder builder;
    std::istringstream in(body);
    std::string errs;
    if (!Json::parseFromStream(builder, in, &root, &errs)) return false;
    return root.isObject();
}

// ---------------------------------------------------------------------------
// shared state
// ---------------------------------------------------------------------------

std::atomic<bool> g_stop{false};
std::atomic<uint64_t> g_next{0};
std::atomic<uint64_t> g_checked{0}, g_plain{0}, g_enc{0}, g_missing{0}, g_errors{0};
std::atomic<uint64_t> g_rejected{0};  // 403/429/5xx — server-side refusals
std::mutex g_print;
std::ofstream g_log;   // timestamped result log (opened in main, see openLog)
std::string g_logPath;

// console window title progress (titleUpdater thread)
std::atomic<bool> g_titleRun{false};
uint64_t g_titleTotal = UINT64_MAX;  // UINT64_MAX = unbounded sequence
Clock::time_point g_titleT0;

BOOL WINAPI ctrlHandler(DWORD) {
    g_stop = true;
    return TRUE;
}

// Refreshes the console window title with run progress:
//   bounded:   NameBrute  done/total (pct)  rate/s  ETA mm:ss
//   unbounded: NameBrute  done  rate/s
void titleUpdater() {
    char buf[384];
    while (g_titleRun.load()) {
        const uint64_t done = g_checked.load();
        const double secs =
            std::chrono::duration<double>(Clock::now() - g_titleT0).count();
        const double rate = secs > 0.25 ? (double)done / secs : 0.0;

        buf[0] = '\0';
        if (g_titleTotal != UINT64_MAX) {
            const double pct =
                g_titleTotal ? 100.0 * (double)done / (double)g_titleTotal : 0.0;
            _snprintf(buf, sizeof(buf), "NameBrute  %llu/%llu  (%.1f%%)  ",
                      (unsigned long long)done, (unsigned long long)g_titleTotal,
                      pct);
            if (rate > 0.5) {
                const uint64_t left = done < g_titleTotal ? g_titleTotal - done : 0;
                const double eta = (double)left / rate;
                char etaBuf[48];
                const long long e = (long long)(eta + 0.5);
                if (e >= 3600)
                    _snprintf(etaBuf, sizeof(etaBuf), "%lldh %02lldm %02llds", e / 3600,
                              (e / 60) % 60, e % 60);
                else if (e >= 60)
                    _snprintf(etaBuf, sizeof(etaBuf), "%lldm %02llds", e / 60, e % 60);
                else
                    _snprintf(etaBuf, sizeof(etaBuf), "%llds", e);
                _snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%llu/s  ETA %s",
                          (unsigned long long)(rate + 0.5), etaBuf);
            }
        } else if (rate > 0.5) {
            _snprintf(buf, sizeof(buf), "NameBrute  %llu  %llu/s",
                      (unsigned long long)done, (unsigned long long)(rate + 0.5));
        } else {
            _snprintf(buf, sizeof(buf), "NameBrute  %llu", (unsigned long long)done);
        }
        SetConsoleTitleA(buf);
        Sleep(500);
    }
}

void printFound(const std::string& name, bool plain, const Json::Value* json,
                size_t bytes, int ms) {
    std::lock_guard lk(g_print);
    if (g_log.is_open()) {  // plain-text mirror in the log file
        std::ostringstream os;
        os << "  " << std::left << std::setw(20) << name
           << (plain ? " [plaintext]" : " [encrypted]");
        if (plain && json) {
            if (json->isMember("buildId"))
                os << "  buildId=" << (*json)["buildId"].asString();
            if (json->isMember("version"))
                os << "  version=" << (*json)["version"].asString();
            if (json->isMember("branch"))
                os << "  branch=" << (*json)["branch"].asString();
        }
        os << "  (" << bytes << " B, " << ms << " ms)\n";
        g_log << os.str();
        g_log.flush();
    }
    if (plain) {
        hue::set_text("green");
        std::cout << "  " << std::left << std::setw(20) << name
                  << " [plaintext]" << std::right;
        if (json) {
            if (json->isMember("buildId"))
                std::cout << "  buildId=" << (*json)["buildId"].asString();
            if (json->isMember("version"))
                std::cout << "  version=" << (*json)["version"].asString();
            if (json->isMember("branch"))
                std::cout << "  branch=" << (*json)["branch"].asString();
        }
    } else {
        hue::set_text("blue");
        std::cout << "  " << std::left << std::setw(20) << name << " [encrypted]" << std::right;
    }
    hue::reset();
    std::cout << "  (" << bytes << " B, " << ms << " ms)\n";
    std::cout.flush();
}

// ---------------------------------------------------------------------------
// timestamped result log: created on start (parameters + short info), found
// names are appended as they appear, summary is appended at the end.
// Location: next to the executable, falling back to the current working dir.
// ---------------------------------------------------------------------------

void openLog(const Config& cfg, const std::string& targetUrl,
             const std::string& src, const std::string& modeLine) {
    std::tm t{};
    const std::time_t now = std::time(nullptr);
    localtime_s(&t, &now);

    char stamp[40];
    std::strftime(stamp, sizeof(stamp), "NameBrute_%Y-%m-%d_%H-%M-%S", &t);

    char exePath[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    const std::filesystem::path exeDir =
        n ? std::filesystem::path(exePath).parent_path()
          : std::filesystem::current_path();

    // Unique name even when two runs start within the same second.
    for (int k = 1; k <= 1000 && !g_log.is_open(); ++k) {
        const std::string fn = std::string(stamp) +
                               (k == 1 ? ".txt" : ("_" + std::to_string(k) + ".txt"));
        for (const std::filesystem::path& dir :
             {exeDir, std::filesystem::current_path()}) {
            const std::filesystem::path file = dir / fn;
            if (std::filesystem::exists(file)) continue;
            g_log.open(file);
            if (g_log.is_open()) {
                g_logPath = file.string();
                break;
            }
        }
    }
    if (!g_log.is_open()) {
        std::cerr << "warning: could not create log file, continuing without log\n";
        return;
    }

    char ts[40];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
    char comp[256];
    DWORD cs = (DWORD)sizeof(comp);
    GetComputerNameA(comp, &cs);
    g_log << "NameBrute run log  " << ts << " (local time)\n"
          << "target:   " << targetUrl << "\n"
          << "app:      " << cfg.app << "\n"
          << "platform: " << cfg.platform << "\n"
          << "api:      " << cfg.apiVer << "\n"
          << "suffix:   " << cfg.suffix << "\n"
          << "source:   " << src << "\n"
          << "mode:     " << modeLine;
    if (cfg.shardN > 1)
        g_log << "  shard=" << (cfg.shardI + 1) << "/" << cfg.shardN;
    g_log << "\n"
          << "machine:  " << comp << "  (logical CPUs: "
          << std::thread::hardware_concurrency() << ")\n"
          << "----\n";
    g_log.flush();
}

void worker(const Config& cfg, uint64_t total,
            const std::function<std::string(uint64_t)>& makeName) {
    httplib::SSLClient cli(cfg.host); // one client per thread (keep-alive)
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(cfg.timeoutMs / 1000, (cfg.timeoutMs % 1000) * 1000);
    cli.set_write_timeout(5, 0);
    cli.set_default_headers({{"User-Agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"}});

    while (!g_stop) {
        uint64_t idx;
        do {
            idx = g_next.fetch_add(1, std::memory_order_relaxed);
        } while (cfg.shardN > 1 &&
                 idx % static_cast<uint64_t>(cfg.shardN) !=
                     static_cast<uint64_t>(cfg.shardI));
        if (idx >= total) break;

        const std::string name = makeName(idx);
        const std::string path =
            "/pipeline/" + cfg.apiVer + "/update/" + cfg.app + "/" +
            cfg.platform + "/" + encodeSegment(name) + "/" + cfg.suffix;

        const auto t0 = Clock::now();
        auto res = cli.Get(path);
        const int ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());

        ++g_checked;
        if (!res) {
            ++g_errors; // network/SSL failure — nothing to report
            continue;
        }
        if (res->status != 200) {
            if (res->status == 403 || res->status == 429 || res->status >= 500)
                ++g_rejected; // server-side refusal — possibly throttling
            else
                ++g_missing;  // 404 etc. — name unavailable, stay silent
            continue;
        }

        Json::Value root;
        if (isJsonObject(res->body, root)) {
            ++g_plain;
            printFound(name, true, &root, res->body.size(), ms);
        } else {
            ++g_enc;
            printFound(name, false, nullptr, res->body.size(), ms);
        }

        if (cfg.delayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.delayMs));
    }
}

// ---------------------------------------------------------------------------
// HTTP/2 mode (vendored nghttp2): each thread owns one TLS connection
// (ALPN h2 + SNI) and keeps up to 128 in-flight streams on it — the
// server hard-caps concurrent streams per connection at 128 (anything
// above is RST_STREAMed immediately). Responses arrive interleaved on
// one socket, so a single connection sustains the full per-IP rate.
// ---------------------------------------------------------------------------

constexpr int kH2MaxStreams = 128;      // server-enforced per-connection cap
constexpr size_t kH2MaxBody = 1u << 20; // body cap per stream (1 MiB)
constexpr int kH2MaxAttempts = 3;       // resubmits per name after a hard close
constexpr int kH2MaxConsecutiveHardErrors = 50;

// Forward declarations of the nghttp2 callbacks (defined below the worker).
static nghttp2_ssize h2WriteCb(nghttp2_session*, const uint8_t*, size_t, int,
                               void*);
static int h2OnFrameRecv(nghttp2_session*, const nghttp2_frame*, void*);
static int h2OnHeader(nghttp2_session*, const nghttp2_frame*, const uint8_t*,
                      size_t, const uint8_t*, size_t, uint8_t, void*);
static int h2OnChunk(nghttp2_session*, uint8_t, int32_t, const uint8_t*,
                     size_t, void*);
static int h2OnStreamClose(nghttp2_session*, int32_t, uint32_t, void*);

struct H2Stream {
    uint64_t idx = 0;
    std::string name;
    int status = 0;  // 0 = no response yet
    std::string body;
    Clock::time_point t0{};
    bool complete = false;
    int attempts = 1;
};

struct H2Worker {
    const Config* cfg = nullptr;
    std::function<std::string(uint64_t)> makeName;
    uint64_t total = 0;

    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    SOCKET sock = INVALID_SOCKET;
    nghttp2_session* sess = nullptr;

    std::unordered_map<int32_t, H2Stream> streams;  // stream id -> state
    std::deque<uint64_t> retryQueue;                // names to resubmit
    std::unordered_map<uint64_t, int> retries;      // idx -> next attempt count
    int consecutiveHardErrors = 0;
    int idleMs = 0;
    bool goawaySeen = false;

    // Sharded global name fetch (same semantics as the HTTP/1.1 worker).
    bool fetchNextIdx(uint64_t& idx) {
        for (;;) {
            const uint64_t v = g_next.fetch_add(1, std::memory_order_relaxed);
            if (cfg->shardN > 1 &&
                v % static_cast<uint64_t>(cfg->shardN) !=
                    static_cast<uint64_t>(cfg->shardI))
                continue;
            if (v >= total) return false;
            idx = v;
            return true;
        }
    }

    bool openConnection() {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(cfg->host.c_str(), "443", &hints, &res) != 0 || !res)
            return false;
        for (addrinfo* a = res; a; a = a->ai_next) {
            sock = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (sock == INVALID_SOCKET) continue;
            if (connect(sock, a->ai_addr, (int)a->ai_addrlen) == 0) break;
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        freeaddrinfo(res);
        if (sock == INVALID_SOCKET) return false;

        int yes = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));

        ssl = SSL_new(ctx);
        if (!ssl) return false;
        SSL_set_fd(ssl, (int)sock);
        SSL_set_tlsext_host_name(ssl, cfg->host.c_str());
        if (SSL_connect(ssl) != 1) {
            SSL_free(ssl);
            ssl = nullptr;
            return false;
        }
        const unsigned char* alpn = nullptr;
        unsigned int alpnLen = 0;
        SSL_get0_alpn_selected(ssl, &alpn, &alpnLen);
        if (alpnLen != 2 || memcmp(alpn, "h2", 2) != 0) {
            SSL_free(ssl);
            ssl = nullptr;
            return false;
        }

        nghttp2_session_callbacks* cbs = nullptr;
        nghttp2_session_callbacks_new(&cbs);
        nghttp2_session_callbacks_set_send_callback2(cbs, &h2WriteCb);
        nghttp2_session_callbacks_set_on_frame_recv_callback(cbs, &h2OnFrameRecv);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cbs, &h2OnChunk);
        nghttp2_session_callbacks_set_on_stream_close_callback(cbs, &h2OnStreamClose);
        nghttp2_session_callbacks_set_on_header_callback(cbs, &h2OnHeader);
        if (nghttp2_session_client_new(&sess, cbs, this) != 0 || !sess) {
            nghttp2_session_callbacks_del(cbs);
            SSL_free(ssl);
            ssl = nullptr;
            return false;
        }
        nghttp2_session_callbacks_del(cbs);  // the session keeps its own copy
        return true;
    }

    // Drop the connection; in-flight names are re-queued (bounded retries).
    void teardown() {
        if (sess) {
            nghttp2_session_del(sess);
            sess = nullptr;
        }
        if (ssl) {
            SSL_free(ssl);
            ssl = nullptr;
        }
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        for (auto& [sid, st] : streams) {
            if (st.complete) continue;
            if (st.attempts < kH2MaxAttempts) {
                retries[st.idx] = st.attempts + 1;
                retryQueue.push_back(st.idx);
            } else {
                ++g_errors;
            }
        }
        streams.clear();
        consecutiveHardErrors = 0;  // a fresh connection starts clean
        goawaySeen = false;
        idleMs = 0;
    }

    bool submitOne(uint64_t idx) {
        H2Stream st;
        st.idx = idx;
        st.name = makeName(idx);
        st.t0 = Clock::now();
        // The retries entry is consumed only on success: a failed submit
        // must not eat the attempt record (the caller re-queues the name).
        if (auto r = retries.find(idx); r != retries.end())
            st.attempts = r->second;
        const std::string path =
            "/pipeline/" + cfg->apiVer + "/update/" + cfg->app + "/" +
            cfg->platform + "/" + encodeSegment(st.name) + "/" + cfg->suffix;
        nghttp2_nv hdrs[6];
        auto setH = [&](int i, const char* name, const char* value) {
            hdrs[i].name =
                const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name));
            hdrs[i].value =
                const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value));
            hdrs[i].namelen = std::strlen(name);
            hdrs[i].valuelen = std::strlen(value);
            hdrs[i].flags = 0;
        };
        setH(0, ":method", "GET");
        setH(1, ":scheme", "https");
        setH(2, ":authority", cfg->host.c_str());
        setH(3, ":path", path.c_str());
        setH(4, "user-agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)");
        setH(5, "accept", "*/*");
        // Returns the new stream id (> 0) or a negative error code.
        const int32_t sid =
            nghttp2_submit_request(sess, nullptr, hdrs, 6, nullptr, nullptr);
        if (sid < 0) return false;
        if (auto r = retries.find(idx); r != retries.end()) retries.erase(r);
        streams[sid] = std::move(st);
        return true;
    }

    void reclaim() {
        for (auto it = streams.begin(); it != streams.end();) {
            if (it->second.complete)
                it = streams.erase(it);
            else
                ++it;
        }
    }
};

static nghttp2_ssize h2WriteCb(nghttp2_session*, const uint8_t* data,
                               size_t length, int, void* ud) {
    auto* w = static_cast<H2Worker*>(ud);
    const int rv = SSL_write(w->ssl, data, (int)length);
    if (rv > 0) return (nghttp2_ssize)rv;
    const int e = SSL_get_error(w->ssl, rv);
    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
        return NGHTTP2_ERR_WOULDBLOCK;
    return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static int h2OnFrameRecv(nghttp2_session*, const nghttp2_frame* frame, void* ud) {
    if (frame->hd.type == NGHTTP2_GOAWAY)
        static_cast<H2Worker*>(ud)->goawaySeen = true;
    return 0;
}

static int h2OnHeader(nghttp2_session*, const nghttp2_frame* frame,
                      const uint8_t* name, size_t namelen,
                      const uint8_t* value, size_t valuelen, uint8_t, void* ud) {
    auto* w = static_cast<H2Worker*>(ud);
    if (frame->hd.type != NGHTTP2_HEADERS) return 0;
    auto it = w->streams.find(frame->hd.stream_id);
    if (it == w->streams.end()) return 0;
    if (namelen == 7 && memcmp(name, ":status", 7) == 0)
        it->second.status = atoi(std::string(reinterpret_cast<const char*>(value),
                                             valuelen).c_str());
    return 0;
}

static int h2OnChunk(nghttp2_session*, uint8_t, int32_t stream_id,
                     const uint8_t* data, size_t len, void* ud) {
    auto* w = static_cast<H2Worker*>(ud);
    auto it = w->streams.find(stream_id);
    if (it == w->streams.end()) return 0;
    auto& b = it->second.body;
    if (b.size() < kH2MaxBody)
        b.append(reinterpret_cast<const char*>(data),
                 std::min(len, kH2MaxBody - b.size()));
    return 0;
}

static int h2OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t, void* ud) {
    auto* w = static_cast<H2Worker*>(ud);
    auto it = w->streams.find(stream_id);
    if (it == w->streams.end()) return 0;
    H2Stream& st = it->second;
    st.complete = true;
    const int ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - st.t0).count());
    // A stream that gets re-queued is not terminal: count the name exactly
    // once, at its final outcome (no double counting of retries).
    const bool terminal = !(st.status == 0 && st.attempts < kH2MaxAttempts);
    if (terminal) ++g_checked;
    if (st.status == 200) {
        w->consecutiveHardErrors = 0;
        Json::Value root;
        if (isJsonObject(st.body, root)) {
            ++g_plain;
            printFound(st.name, true, &root, st.body.size(), ms);
        } else {
            ++g_enc;
            printFound(st.name, false, nullptr, st.body.size(), ms);
        }
    } else if (st.status != 0) {
        w->consecutiveHardErrors = 0;
        if (st.status == 403 || st.status == 429 || st.status >= 500)
            ++g_rejected; // server-side refusal — possibly throttling
        else
            ++g_missing;  // 404 etc. — unavailable, stay silent
    } else {
        // Closed before any response (reset/cancelled): always retry the
        // name while attempts remain; count it as an error only once
        // attempts are exhausted (so it is never silently skipped).
        ++w->consecutiveHardErrors;
        if (st.attempts < kH2MaxAttempts) {
            w->retries[st.idx] = st.attempts + 1;
            w->retryQueue.push_back(st.idx);
        } else {
            ++g_errors;
        }
    }
    return 0;
}

void workerH2(const Config& cfg, uint64_t total,
              const std::function<std::string(uint64_t)>& makeName) {
    H2Worker w;
    w.cfg = &cfg;
    w.total = total;
    w.makeName = makeName;

    w.ctx = SSL_CTX_new(TLS_client_method());
    // Serialized ALPN list: 1 length byte + "h2" (3 bytes total).
    SSL_CTX_set_alpn_protos(w.ctx,
                            reinterpret_cast<const uint8_t*>(NGHTTP2_PROTO_ALPN),
                            NGHTTP2_PROTO_ALPN_LEN);

    int backoffMs = 0;
    while (!g_stop) {
        if (!w.sess) {
            if (!w.openConnection()) {
                backoffMs = backoffMs ? std::min(backoffMs * 2, 2000) : 100;
                if (g_stop) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                continue;
            }
            backoffMs = 0;
        }

        // Fill in-flight streams up to the server cap (retries first).
        bool noMore = false;
        bool tornDown = false;
        while (static_cast<int>(w.streams.size()) < kH2MaxStreams) {
            uint64_t idx = 0;
            if (!w.retryQueue.empty()) {
                idx = w.retryQueue.front();
                w.retryQueue.pop_front();
            } else if (!w.fetchNextIdx(idx)) {
                noMore = true;
                break;
            }
            if (!w.submitOne(idx)) {
                // A failed submit consumes an attempt but must not lose the
                // name: re-queue it (give up only after real attempts).
                auto r = w.retries.find(idx);
                const int a = (r != w.retries.end()) ? r->second : 1;
                if (a < kH2MaxAttempts) {
                    w.retries[idx] = a + 1;
                    w.retryQueue.push_back(idx);
                } else {
                    ++g_errors;
                }
                w.teardown();
                tornDown = true;
                break;
            }
        }
        if (tornDown) continue;
        if (noMore && w.streams.empty()) break;  // every name handled

        if (nghttp2_session_send(w.sess) < 0) {
            w.teardown();
            continue;
        }

        // Block for at most 1 s, then service the session again.
        uint8_t buf[65536];
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(w.sock, &rfds);
        timeval tv{1, 0};
        const int sel = select(0, &rfds, nullptr, nullptr, &tv);
        bool gotData = false;
        if (sel > 0) {
            const int n = SSL_read(w.ssl, buf, sizeof(buf));
            if (n > 0) {
                gotData = true;
                // mem_recv may consume only part of the buffer (bytes
                // consumed on success, <0 on error); feed the rest until
                // fully processed. A 0-consume stall is a protocol failure.
                uint8_t* p = buf;
                size_t left = (size_t)n;
                bool recvOk = true;
                while (left > 0) {
                    const ptrdiff_t mr = nghttp2_session_mem_recv(w.sess, p, left);
                    if (mr <= 0) {
                        recvOk = false;
                        break;
                    }
                    p += (size_t)mr;
                    left -= (size_t)mr;
                }
                if (!recvOk || nghttp2_session_send(w.sess) < 0) {
                    w.teardown();
                    continue;
                }
            } else {
                const int e = SSL_get_error(w.ssl, n);
                if (e != SSL_ERROR_WANT_READ) {
                    w.teardown();  // reset/closed/unrecoverable
                    continue;
                }
            }
        }
        if (gotData) {
            w.idleMs = 0;
        } else {
            w.idleMs += 1000;
            if (!w.streams.empty() && w.idleMs >= cfg.timeoutMs) {
                w.teardown();  // hung connection; names get re-queued
                continue;
            }
            if (w.goawaySeen && w.streams.empty() && w.retryQueue.empty() &&
                !noMore) {
                w.teardown();  // server closed; reconnect to continue
            }
        }
        w.reclaim();
    }

    if (w.sess) nghttp2_session_del(w.sess);
    if (w.ssl) SSL_free(w.ssl);
    if (w.sock != INVALID_SOCKET) closesocket(w.sock);
    if (w.ctx) SSL_CTX_free(w.ctx);
}

void printSummary(const Config& cfg, bool stopped, Clock::time_point t0) {
    const double secs =
        std::chrono::duration<double>(Clock::now() - t0).count();
    const uint64_t checked = g_checked.load();
    const std::string targetUrl =
        "https://" + cfg.host + "/pipeline/" + cfg.apiVer + "/update/" + cfg.app +
        "/" + cfg.platform + "/<name>/" + cfg.suffix;
    const std::string rate =
        secs > 0
            ? (" in " + std::to_string(secs).substr(0, 5) + " s (" +
               std::to_string(checked / secs).substr(0, 4) + "/s)")
            : std::string();
    std::lock_guard lk(g_print);
    hue::set_text("grey");
    std::cout << "\n----\n";
    if (stopped) std::cout << "stopped (Ctrl+C)\n";
    std::cout << "target:  " << targetUrl << "\n"
              << "checked: " << checked << rate << "\n"
              << "  plaintext:  " << g_plain.load() << "\n"
              << "  encrypted:  " << g_enc.load() << "\n"
              << "  unavailable:" << " " << g_missing.load() << "\n";
    if (g_rejected.load() > 0)
        std::cout << "  rejected(403/429/5xx): " << g_rejected.load()
                  << "  (possible server throttling — consider re-running)\n";
    std::cout << "  errors:     " << g_errors.load() << "\n";
    hue::reset();
    if (g_log.is_open()) {
        g_log << "\n----\n";
        if (stopped) g_log << "stopped (Ctrl+C)\n";
        g_log << "target:  " << targetUrl << "\n"
              << "checked: " << checked << rate << "\n"
              << "  plaintext:  " << g_plain.load() << "\n"
              << "  encrypted:  " << g_enc.load() << "\n"
              << "  unavailable:" << " " << g_missing.load() << "\n";
        if (g_rejected.load() > 0)
            g_log << "  rejected(403/429/5xx): " << g_rejected.load()
                  << "  (possible server throttling — consider re-running)\n";
        g_log << "  errors:     " << g_errors.load() << "\n";
        g_log.flush();
    }
}

} // namespace

int main(int argc, char** argv) {
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);  // also used directly by -h2 mode

    Config cfg;
    parseArgs(argc, argv, cfg);

    std::vector<std::string> names;
    uint64_t total = 0;

    if (cfg.sequenceMode) {
        total = sequenceTotal(cfg.maxLen, cfg.charset);
        names.clear();
    } else if (!cfg.file.empty()) {
        names = loadNamesFile(cfg.file);
        total = static_cast<uint64_t>(names.size());
    } else {
        names = cfg.args;
        total = static_cast<uint64_t>(names.size());
    }

    if (cfg.threads == 0) {
        unsigned hc = std::thread::hardware_concurrency();
        cfg.threads = (hc >= 1 && hc <= 256) ? static_cast<int>(hc) : 4;
    }

    const std::string src = cfg.sequenceMode
        ? (std::to_string(total == UINT64_MAX ? 0 : total) + " generated names (charset '" +
           cfg.charset + "', length 1..") +
          (cfg.maxLen == 0 ? std::string("*") : std::to_string(cfg.maxLen)) + ")"
        : (std::to_string(total) + " names from " +
           (cfg.file.empty() ? "arguments" : cfg.file));

    const std::string targetUrl =
        "https://" + cfg.host + "/pipeline/" + cfg.apiVer + "/update/" + cfg.app +
        "/" + cfg.platform + "/<name>/" + cfg.suffix;
    const std::string modeLine = cfg.h2Mode
        ? ("HTTP/2, " + std::to_string(cfg.threads) + " connections x up to " +
           std::to_string(kH2MaxStreams) + " in-flight streams each  timeout=" +
           std::to_string(cfg.timeoutMs) + " ms")
        : (std::to_string(cfg.threads) + "  timeout=" +
           std::to_string(cfg.timeoutMs) + " ms  delay=" +
           std::to_string(cfg.delayMs) + " ms");
    openLog(cfg, targetUrl, src, modeLine);

    std::cout << "NameBrute: https://" << cfg.host << "  api=" << cfg.apiVer
              << "  app=" << cfg.app
              << "  platform=" << cfg.platform << "  suffix=" << cfg.suffix << "\n"
              << "source:  " << src << "\n"
              << (cfg.h2Mode ? "mode:    " : "threads: ") << modeLine
              << (cfg.shardN > 1 ? ("  shard=" + std::to_string(cfg.shardI + 1) + "/" +
                                    std::to_string(cfg.shardN)).c_str()
                                 : "")
              << "\n"
              << "(green = plaintext response, blue = encrypted, unavailable = silent)\n";
    if (!cfg.h2Mode && cfg.threads > 4096)
        std::cout << "note:    >4096 connections approaches the per-process handle\n"
                     "         limit — to go further, run several instances with -shard\n";
    if (cfg.h2Mode && cfg.threads > 128)
        std::cout << "note:    more than ~128 connections rarely raises the per-IP\n"
                     "         rate (server caps streams); run several instances\n"
                     "         with -shard across different egress IPs instead\n";
    if (g_log.is_open())
        std::cout << "log:     " << g_logPath << "\n";

    const auto t0 = Clock::now();
    g_titleT0 = t0;
    g_titleTotal = total;
    g_titleRun = true;
    std::thread titleTh(titleUpdater);
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(cfg.threads));
    for (int i = 0; i < cfg.threads; ++i) {
        pool.emplace_back([&cfg, total, &names]() {
            const auto makeName = [&](uint64_t idx) -> std::string {
                return cfg.sequenceMode ? sequenceName(idx + 1, cfg.charset)
                                        : names[idx];
            };
            if (cfg.h2Mode)
                workerH2(cfg, total, makeName);
            else
                worker(cfg, total, makeName);
        });
    }
    for (auto& t : pool) t.join();
    g_titleRun = false;
    titleTh.join();
    printSummary(cfg, g_stop.load(), t0);
    return 0;
}

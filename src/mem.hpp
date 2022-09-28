#ifndef PARNASSIA_TRINERVIS_MEM_HPP
#define PARNASSIA_TRINERVIS_MEM_HPP
#include <fstream>
#include <string>
#include "logger.h"

namespace MEM {
constexpr auto SSmax = std::numeric_limits<std::streamsize>::max();
class memInfo {
public:
    memInfo();
    ~memInfo() = default;
    std::string getMemUsage();
    std::string getMb(int64_t size);
private:
    void getMemInfo();
    const std::string procFile = "/proc/meminfo";
public:
    int64_t mem_total{};
    int64_t mem_available{};
    int64_t mem_free{};
    int64_t mem_cache{};
};

std::string memInfo::getMb(int64_t size)
{
    size >>= 20;
    std::stringstream ss;
    ss << size << "Mb";
    return ss.str();
}

std::string memInfo::getMemUsage()
{
    if (!mem_total || mem_available >= mem_total) {
        LOG->error("Get mem info error");
    }
    std::stringstream ss;
    ss << (mem_total - mem_available) * 100 / mem_total;
    return ss.str();
}

memInfo::memInfo()
{
    getMemInfo();
}

void memInfo::getMemInfo()
{
    std::ifstream meminfo(procFile);
    if (meminfo.good()) {
        bool got_avail = false;
        for (std::string label; meminfo.peek() != 'D' && meminfo >> label;) {
            if (label == "MemTotal:") {
                meminfo >> mem_total;
                mem_total <<= 10;
            }
            if (label == "MemFree:") {
                meminfo >> mem_free;
                mem_free <<= 10;
            }
            else if (label == "MemAvailable:") {
                meminfo >> mem_available;
                mem_available <<= 10;
                got_avail = true;
            }
            else if (label == "Cached:") {
                meminfo >> mem_cache;
                mem_cache <<= 10;
            }
            meminfo.ignore(SSmax, '\n');
        }
        if (not got_avail) {
            mem_available = mem_free + mem_cache;
        }
    } else {
        LOG->warn("Failed to read /proc/meminfo");
    }
    meminfo.close();
}

}
#endif   // PARNASSIA_TRINERVIS_MEM_HPP

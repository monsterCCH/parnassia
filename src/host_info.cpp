#include "host_info.h"
#include "cpu.hpp"
#include "mem.hpp"
#include "get_adapters.h"
#include "utils_file.h"
#include "utils_string.h"
#include "nlohmann/json.hpp"


string hostInfo::getCpuUsage()
{
    return CPU::getCpuUsage();
}

hostInfo::hostInfo()
{
    if (!file_exists(procPath)) {
        LOG->error("Proc filesystem not found or no permission, host info can't get");
    }
    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) {
        cpu_count = 1;
        LOG->warn("Could not determine number of cores, defaulting to 1");
    }
    sys_id = getDmiValue("/sys/class/dmi/id/product_uuid");
    cpu_usage = getCpuUsage();
    net_ipv4 = getIpAddress();
    getMemInfo();
}

void hostInfo::getMemInfo()
{
    MEM::memInfo mi;
    mem_usage = mi.getMemUsage();
    mem_available = mi.getMb(mi.mem_available);
    mem_total = mi.getMb(mi.mem_total);
}

void hostInfo::flush()
{
    cpu_usage = getCpuUsage();
    getMemInfo();
    net_ipv4 = getIpAddress();
}
std::string hostInfo::getDmiValue(const char* dmi_id)
{
    std::string res;
    try {
        res = toupper_copy(trim_copy(get_file_contents(dmi_id, 256)));
        char last_char = res[res.length() - 1];
        if (last_char == '\r' || last_char == '\n') {
            res = res.erase(res.length() - 1);
        }
    } catch (const std::exception& e) {
        LOG->warn("Can not read dmi_info : {}", dmi_id);
    }
    return res;
}
string hostInfo::genHwInfoJson()
{
    nlohmann::json js;
    js["sysId"] = sys_id;
    js["cpuPercent"] = cpu_usage;
    js["cpuPoint"] = cpu_count;
    js["memPercent"] = mem_usage;
    js["memTotal"] = mem_total;
    js["memAvail"] = mem_available;
    js["systemIp"] = net_ipv4;
    return js.dump();
}
string hostInfo::getIpAddress()
{
    NET::NetAdapterInfo adapter_info;
    if (getDefAdapterInfo(adapter_info) != FUNC_RET_OK) {
        return {};
    }
    return NET::ipv4ToString(adapter_info.ipv4_address);
}

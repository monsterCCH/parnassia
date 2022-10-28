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
    if (!dir_exists(procPath)) {
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
    disk_info = DISK::getDiskInfos();
}

void hostInfo::getMemInfo()
{
    MEM::memInfo mi;
    mem_usage = mi.getMemUsage();
    mem_available = mi.getMb(mi.mem_available);
    mem_total = mi.getMb(mi.mem_total);
}

void hostInfo::flushHwInfo()
{
    cpu_usage = getCpuUsage();
    getMemInfo();
    net_ipv4 = getIpAddress();
    disk_info = DISK::getDiskInfos();
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
    js[InfoItemMap[II_SYS_ID]] = sys_id;
    js[InfoItemMap[II_NET_ADDR]] = net_ipv4;
    js[InfoItemMap[II_CPU_USAGE]] = cpu_usage + "%";
    js[InfoItemMap[II_CPU_COUNT]] = cpu_count;
    js[InfoItemMap[II_MEM_USAGE]] = mem_usage + "%";
    js[InfoItemMap[II_MEM_TOTAL]] = mem_total;
    js[InfoItemMap[II_MEM_AVAILABLE]] = mem_available;

    nlohmann::json::array_t disk_array;
    for (auto& iter : disk_info) {
        nlohmann::json array = nlohmann::json::object();
        array.push_back({InfoItemMap[II_DISK_MOUNT], iter.monunt_point});
        array.push_back({InfoItemMap[II_DISK_TOTAL], to_string(iter.total / MB) + "Mib"});
        array.push_back({InfoItemMap[II_DISK_AVAILABLE], to_string(iter.free / MB) + "Mib"});
        array.push_back({InfoItemMap[II_DISK_USAGE], to_string(iter.used_percent) + "%"});
        js["diskInfo"].push_back(array);
    }
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
string hostInfo::genDockerInfoJson()
{
    string res = getCmdResult("docker info --format '{{json .}}'");
    try {
        nlohmann::json js = nlohmann::json::parse(res);
        js[InfoItemMap[II_SYS_ID]] = sys_id;
        return js.dump();
    } catch (exception& e) {
        LOG->warn("docker info json parser error : {}", e.what());
    }
    return {};
//    return getCmdResult("docker info --format '{{json .}}'");

}
string hostInfo::genDockerImageJson()
{
    // 不要调整格式，换行是为了输入\n
    string res = getCmdResult(R"(echo "[$(IFS=$'
'; for line in $(docker images --no-trunc --format="{{json .}}"); do echo -n "${line},"; done;)]")");
    res = res.substr(0, res.find_last_of(',')) + "]";
    try {
        nlohmann::json::array_t array = nlohmann::json::parse(res);
        nlohmann::json js;
        js["docker_image"] = array;
        js[InfoItemMap[II_SYS_ID]] = sys_id;
        return js.dump();
    } catch (exception& e) {
        LOG->warn("docker image json parser error : {}", e.what());
    }
    return {};
}
string hostInfo::genDockerContainerJson()
{
    // 不要调整格式，换行是为了输入\n
    string res = getCmdResult(R"(echo "[$(IFS=$'
'; for line in $(docker container ls -a --no-trunc --format="{{json .}}"); do echo -n "${line},"; done;)]")");
    res = res.substr(0, res.find_last_of(',')) + "]";
    try {
        nlohmann::json::array_t array = nlohmann::json::parse(res);
        nlohmann::json js;
        js["docker_container"] = array;
        js[InfoItemMap[II_SYS_ID]] = sys_id;
        return js.dump();
    } catch (exception& e) {
        LOG->warn("docker container json parser error : {}", e.what());
    }
    return {};
}
string hostInfo::getCmdResult(const string& cmd)
{
    stringstream ss;
    FILE *fp = NULL;
    char buf[1024]={0};
    fp = popen(cmd.c_str(), "r");
    if(fp) {
        while (fgets(buf, sizeof(buf), fp) != nullptr) {
            int size = strlen(buf);
            if (buf[size - 1] == '\n' || buf[size - 1] == '\r') {
                buf[size - 1] = 0;
            }
            ss << buf;
        }
        pclose(fp);
    }
    return ss.str();
}

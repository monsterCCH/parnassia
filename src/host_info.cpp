#include "host_info.h"
#include "openssl/md5.h"
#include "cpu.hpp"
#include "mem.hpp"
#include "get_adapters.h"
#include "utils_file.h"
#include "utils_string.h"
#include "nlohmann/json.hpp"

[[maybe_unused]]const unordered_map<string, VIRTUALIZATION_DETAIL> virtual_cpu_names{
    {"bhyve bhyve ", V_OTHER},
    {"KVM", KVM},
    {"MICROSOFT", HV},
    {"lrpepyh vr", HV},
    {"prl hyperv  ", PARALLELS},
    {"VMWARE", VMWARE},
    {"XenVMMXenVMM", V_XEN},
    {"ACRNACRNACRN", V_OTHER},
    {"VBOX", VIRTUALBOX}};

const unordered_map<string, VIRTUALIZATION_DETAIL> vm_vendors{
    {"VMWARE", VMWARE},
    {"MICROSOFT", HV},
    {"PARALLELS", PARALLELS},
    {"VITRUAL MACHINE", V_OTHER},
    {"INNOTEK GMBH", VIRTUALBOX},
    {"POWERVM", V_OTHER},
    {"BOCHS", V_OTHER},
    {"QEMU", QEMU},
    {"KVM", KVM}};

static VIRTUALIZATION_DETAIL find_in_map(const unordered_map<string, VIRTUALIZATION_DETAIL>& map,
                                                 const string& data) {
    for (auto it : map) {
        if (data.find(it.first) != string::npos) {
            return it.second;
        }
    }
    return BARE_TO_METAL;
}

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
    sys_id = genSysId();
    cpu_usage = getCpuUsage();
    net_ipv4 = getIpAddress();
    mac_addr = getMacAddress();
    getMemInfo();
    disk_info = DISK::getDiskInfos();
    cur_env = getRunningEnv();
}

string hostInfo::genSysId()
{
    string sys_uuid = m_dmi_info.sys_uuid();
    string mac = getMacAddress();
    string serials = sys_uuid + "-" + mac;
    unsigned char md5_result[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(serials.c_str()), serials.length(), md5_result);

    std::string md5_hex;
    const char map[] = "0123456789abcdef";

    for (size_t i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        md5_hex += map[md5_result[i] / 16];
        md5_hex += map[md5_result[i] % 16];
    }

    return md5_hex;
}

CONTAINER_TYPE hostInfo::getRunningEnv()
{
    CONTAINER_TYPE result;
    if (isDocker()) {
        return CONTAINER_TYPE::DOCKER;
    }

    if (getVirtualDetail() != BARE_TO_METAL || isCloud()) {
        return CONTAINER_TYPE::VM;
    }
    return CONTAINER_TYPE::NONE;
}

VIRTUALIZATION_DETAIL hostInfo::getVirtualDetail()
{
    VIRTUALIZATION_DETAIL result = BARE_TO_METAL;
    const string bios_description = m_dmi_info.bios_description();
    const string bios_vendor = m_dmi_info.bios_vendor();
    const string sys_vendor = m_dmi_info.sys_vendor();
    if ((result = find_in_map(vm_vendors, bios_description)) == BARE_TO_METAL) {
        if ((result = find_in_map(vm_vendors, bios_vendor)) == BARE_TO_METAL) {
            result = find_in_map(vm_vendors, sys_vendor);
        }
    }
    return result;
}

bool hostInfo::isCloud()
{
    CLOUD_PROVIDER result = PROV_UNKNOWN;
    const string bios_description = m_dmi_info.bios_description();
    const string bios_vendor = m_dmi_info.bios_vendor();
    const string sys_vendor = m_dmi_info.sys_vendor();
    if (!bios_description.empty() || !bios_vendor.empty() || !sys_vendor.empty()) {
        if (bios_vendor.find("SEABIOS") != string::npos || bios_description.find("ALIBABA") != string::npos ||
            sys_vendor.find("ALIBABA") != string::npos) {
            result = ALI_CLOUD;
        } else if (sys_vendor.find("GOOGLE") != string::npos ||
                 bios_description.find("GOOGLECOMPUTEENGINE") != string::npos) {
            result = GOOGLE_CLOUD;
        } else if (bios_vendor.find("AWS") != string::npos || bios_description.find("AMAZON") != string::npos ||
                 sys_vendor.find("AWS") != string::npos) {
            result = AWS;
        } else if (bios_description.find("HP-COMPAQ") != string::npos ||
                 bios_description.find("ASUS") != string::npos || bios_description.find("DELL") != string::npos) {
            result = ON_PREMISE;
        }
    }
    return result != ON_PREMISE && result != PROV_UNKNOWN;
}

bool hostInfo::isDocker()
{
    CONTAINER_TYPE result;
    FILE *fp;
    char *line = nullptr;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/self/cgroup", "r");
    if (!fp) {
        while ((read = getline(&line, &len, fp)) != -1
               && result == CONTAINER_TYPE::NONE) {
            if (strstr(line, "docker") != nullptr) {
                result = CONTAINER_TYPE::DOCKER;
            }
        }
        if (line) {
            free(line);
        }
        fclose(fp);
    }

    if (result == CONTAINER_TYPE::NONE) {
        ifstream dockerEnv("/.dockerenv");
        if (dockerEnv.good()) {
            return true;
        }
    }else {
        return true;
    }
    return false;
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
    mac_addr = getMacAddress();
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
    string os_type = to_string(cur_env + 1);
    nlohmann::json js;
    js[InfoItemMap[II_SYS_ID]] = sys_id;
    js[InfoItemMap[II_OS_TYPE]] = os_type;
    js[InfoItemMap[II_NET_ADDR]] = net_ipv4;
    js[InfoItemMap[II_NET_MAC]] = mac_addr;
    js[InfoItemMap[II_CPU_USAGE]] = cpu_usage + "%";
    js[InfoItemMap[II_CPU_COUNT]] = cpu_count;
    js[InfoItemMap[II_MEM_USAGE]] = mem_usage + "%";
    js[InfoItemMap[II_MEM_TOTAL]] = mem_total;
    js[InfoItemMap[II_MEM_AVAILABLE]] = mem_available;

    unordered_map<string, NET::NetAdapterInfo> name_adapter;
    if (FUNC_RET_ERROR == getAdaptersInfo(name_adapter)) {
        LOG->warn("get adapter info error");
    }
    for (auto &it : name_adapter) {
        nlohmann::json array = nlohmann::json::object();
        array.push_back({InfoItemMap[II_NET_NAME], it.first});
        array.push_back({InfoItemMap[II_NET_MAC], NET::macToString(it.second.mac_address)});
        array.push_back({InfoItemMap[II_NET_ADDR], NET::ipv4ToString(it.second.ipv4_address)});
        js["adapter"].push_back(array);
    }

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

string hostInfo::getMacAddress()
{
    NET::NetAdapterInfo adapter_info;
    if (getDefAdapterInfo(adapter_info) != FUNC_RET_OK) {
        return {};
    }
    return NET::macToString(adapter_info.mac_address);
}

string hostInfo::genDockerInfoJson()
{
    string res = getCmdResult("docker info --format '{{json .}}'");
    try {
        nlohmann::json js = nlohmann::json::parse(res);
        js[InfoItemMap[II_SYS_ID]] = sys_id;
        return js.dump();
    } catch (exception& e) {
        LOG->debug("docker info json parser error : {}", e.what());
    }
    return {};
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
        LOG->debug("docker image json parser error : {}", e.what());
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
        LOG->debug("docker container json parser error : {}", e.what());
    }
    return {};
}

string hostInfo::genDockerContainerStatsJson()
{
    // 不要调整格式，换行是为了输入\n
    string res = getCmdResult(R"(echo "[$(IFS=$'
'; for line in $(docker container stats --no-stream --no-trunc --format="{{json .}}"); do echo -n "${line},"; done;)]")");
    res = res.substr(0, res.find_last_of(',')) + "]";
    try {
        nlohmann::json::array_t array = nlohmann::json::parse(res);
        nlohmann::json js;
        js["docker_container_stats"] = array;
        js[InfoItemMap[II_SYS_ID]] = sys_id;
        return js.dump();
    } catch (exception& e) {
        LOG->debug("docker container json parser error : {}", e.what());
    }
    return {};
}

string hostInfo::genKvmInfoJson()
{
    string res = getCmdResult(R"(/usr/bin/kvmtop -r 1 --host --cpu --mem --disk --net --io --printer=json)");
    return res;
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

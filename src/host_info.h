#ifndef PARNASSIA_TRINERVIS_HOST_INFO_H
#define PARNASSIA_TRINERVIS_HOST_INFO_H
#include <string>
#include <vector>
#include <map>
#include "disk.h"
#include "dmi_info.h"

using namespace std;

enum InfoItem {
    II_SYS_ID = 0,
    II_OS_TYPE,
    II_NET_NAME,
    II_NET_ADDR,
    II_NET_MAC,
    II_CPU_USAGE,
    II_CPU_COUNT,
    II_MEM_USAGE,
    II_MEM_TOTAL,
    II_MEM_AVAILABLE,
    II_DISK_MOUNT,
    II_DISK_TOTAL,
    II_DISK_AVAILABLE,
    II_DISK_USAGE
};

static std::map<int, std::string> InfoItemMap = {
    {II_SYS_ID, "sysId"},
    {II_OS_TYPE, "osType"},
    {II_NET_NAME, "netName"},
    {II_NET_ADDR, "netAddr"},
    {II_NET_MAC, "MAC"},
    {II_CPU_USAGE, "cpuPercent"},
    {II_CPU_COUNT, "cpuCount"},
    {II_MEM_USAGE, "memPercent"},
    {II_MEM_TOTAL, "memTotal"},
    {II_MEM_AVAILABLE, "memAvail"},
    {II_DISK_MOUNT, "diskMount"},
    {II_DISK_TOTAL, "diskTotal"},
    {II_DISK_AVAILABLE, "diskAvail"},
    {II_DISK_USAGE, "diskPercent"}
};

enum CONTAINER_TYPE {NONE, VM, DOCKER};

typedef enum { BARE_TO_METAL, VMWARE, VIRTUALBOX, V_XEN, QEMU, KVM, HV, PARALLELS, V_OTHER } VIRTUALIZATION_DETAIL;

typedef enum {
    PROV_UNKNOWN = 0,
    ON_PREMISE = 1,
    GOOGLE_CLOUD = 2,
    AZURE_CLOUD = 3,
    AWS = 4,
    ALI_CLOUD = 5
} CLOUD_PROVIDER;


class hostInfo
{
public:
    hostInfo();
    ~hostInfo() = default;
    void   flushHwInfo();
    string genHwInfoJson();
    string genDockerInfoJson();
    string genDockerImageJson();
    string genDockerContainerJson();
    string genDockerContainerStatsJson();
    string genKvmInfoJson();
    static std::string getDmiValue(const char* dmi_id);
    CONTAINER_TYPE getRunningEnv();
    bool isDocker();
    bool isCloud();
    VIRTUALIZATION_DETAIL getVirtualDetail();
    string getSysId() const { return sys_id; }
    string getIpAddr() const { return net_ipv4; }

private:
    const dmiInfo m_dmi_info;
    string getCpuUsage();
    void getMemInfo();
    string getIpAddress();
    string getMacAddress();
    string getCmdResult(const string& cmd);
    string genSysId();
    vector<string> ip_set;
    string sys_id;
    string os_name;
    string net_ipv4;
    string mac_addr;
    string cpu_usage;
    string mem_usage;
    string mem_available;
    string mem_total;
    vector<DISK::DiskInfo> disk_info;
    CONTAINER_TYPE cur_env;

    long         cpu_count;
    const string procPath = "/proc";
    long clk_tck;
    long page_size;
};



#endif   // PARNASSIA_TRINERVIS_HOST_INFO_H

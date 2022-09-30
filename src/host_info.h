#ifndef PARNASSIA_TRINERVIS_HOST_INFO_H
#define PARNASSIA_TRINERVIS_HOST_INFO_H
#include <string>
#include <vector>
#include <map>
#include "disk.h"

using namespace std;

enum InfoItem {
    II_SYS_ID = 0,
    II_NET_ADDR,
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
    {II_NET_ADDR, "netAddr"},
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

private:
    string getCpuUsage();
    void getMemInfo();
    string getIpAddress();
    string getCmdResult(const string& cmd);
    std::string getDmiValue(const char* dmi_id);
    vector<string> ip_set;
    string sys_id;
    string os_type;
    string os_name;
    string net_ipv4;
    string cpu_usage;
    string mem_usage;
    string mem_available;
    string mem_total;
    vector<DISK::DiskInfo> disk_info;

    long         cpu_count;
    const string procPath = "/proc";
    long clk_tck;
    long page_size;
};



#endif   // PARNASSIA_TRINERVIS_HOST_INFO_H

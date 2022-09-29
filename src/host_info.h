#ifndef PARNASSIA_TRINERVIS_HOST_INFO_H
#define PARNASSIA_TRINERVIS_HOST_INFO_H
#include <string>
#include <vector>
#include "disk.h"

using namespace std;

class hostInfo
{
public:
    hostInfo();
    ~hostInfo() = default;
    void flush();
    string genHwInfoJson();

private:
    string getCpuUsage();
    void getMemInfo();
    string getIpAddress();
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

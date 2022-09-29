#include "get_adapters.h"
#include <unordered_map>
#include <string>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <fstream>
#include "logger.h"
#include "utils_string.h"
using namespace std;

BEGIN_NAMESPACE(NET)
const string NET_ROUTE_FILEPATH = "/proc/net/route";

FUNCTION_RETURN getAdaptersInfo(unordered_map<string, NetAdapterInfo>& name_adapter)
{


    FUNCTION_RETURN ret = FUNC_RET_OK;
    struct ifaddrs *ifaddr, *ifa;
    int family, n = 0;
    unsigned int if_num;

    if (getifaddrs(&ifaddr) == -1) {
        LOG->warn("NET getifaddrs failed");
        return FUNC_RET_ERROR;
    }

    for (ifa = ifaddr, n = 0, if_num = 0; ifa != nullptr; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == nullptr || (ifa->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        string if_name(ifa->ifa_name, mstrnlen_s(ifa->ifa_name, NI_MAXHOST));
        NetAdapterInfo *currentAdapter;
        if (name_adapter.find(if_name) == name_adapter.end()) {
            NetAdapterInfo newAdapter;
            memset(&newAdapter, 0, sizeof(NetAdapterInfo));
            mstrlcpy(&newAdapter.description[0], ifa->ifa_name, NI_MAXHOST);
            name_adapter[if_name] = newAdapter;
        }

        auto it = name_adapter.find(if_name);
        currentAdapter = &it->second;
        family = ifa->ifa_addr->sa_family;
        LOG->debug("Net get adapter: {} {} {0:d}", ifa->ifa_name,
                   (family == AF_PACKET) ? "AF_PACKET"
                   : (family == AF_INET) ? "AF_INET" : (family == AF_INET6) ? "AF_INET6" : "???",
                   family);

        if (family == AF_INET) {
            auto *s1 = (struct sockaddr_in *)ifa->ifa_addr;
            in_addr_t iaddr = s1->sin_addr.s_addr;
            currentAdapter->ipv4_address[0] = (iaddr & 0x000000ff);
            currentAdapter->ipv4_address[1] = (iaddr & 0x0000ff00) >> 8;
            currentAdapter->ipv4_address[2] = (iaddr & 0x00ff0000) >> 16;
            currentAdapter->ipv4_address[3] = (iaddr & 0xff000000) >> 24;

        }
        if (family == AF_PACKET && ifa->ifa_data != NULL) {
            auto *s1 = (struct sockaddr_ll *)ifa->ifa_addr;
            int i;
            for (i = 0; i < 6; i++) {
                currentAdapter->mac_address[i] = s1->sll_addr[i];
            }
        }
    }
    freeifaddrs(ifaddr);

    if (name_adapter.empty()) {
        ret = FUNC_RET_ERROR;
    } else {
        ret = FUNC_RET_OK;
    }
    return ret;
}

string getDefInterface() {
    string defaultInterface;

    ifstream routeFile(NET_ROUTE_FILEPATH, ios_base::in);
    if (!routeFile.good()) {
        return {};
    }

    string line;
    vector<string> tokens;
    while(getline(routeFile, line)) {
        istringstream stream(line);
        copy(istream_iterator<string>(stream),
             istream_iterator<string>(),
             back_inserter<vector<string> >(tokens));

        if ((tokens.size() >= 2) && (tokens[1] == string("00000000")))
        {
            defaultInterface = tokens[0];
            break;
        }
        tokens.clear();
    }
    routeFile.close();

    return defaultInterface;
}

FUNCTION_RETURN getDefAdapterInfo(NetAdapterInfo& info)
{
    unordered_map<string, NetAdapterInfo> name_adapter;
    if (FUNC_RET_ERROR == getAdaptersInfo(name_adapter)) {
        return {};
    }
    string def_interface = getDefInterface();

    for (auto &it : name_adapter) {
        if (it.first == def_interface) {
            info = it.second;
            return FUNC_RET_OK;
        }
    }

    for (auto &it : name_adapter) {
        if (!memcmp(it.second.description, "eth", 3) ||
            !memcmp(it.second.description, "ens", 3) ||
            !memcmp(it.second.description, "enp", 3) )  {
            info = it.second;
            return FUNC_RET_OK;
        }
    }
    return FUNC_RET_ERROR;
}

std::string ipv4ToString(unsigned char ipv4[])
{
    stringstream ss;
    ss << static_cast<unsigned int>(ipv4[0]) << "."
       << static_cast<unsigned int>(ipv4[1]) << "."
       << static_cast<unsigned int>(ipv4[2]) << "."
       << static_cast<unsigned int>(ipv4[3]) ;
    return ss.str();
}
END_NAMESPACE

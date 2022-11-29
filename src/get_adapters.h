#ifndef PARNASSIA_TRINERVIS_GET_ADAPTERS_H
#define PARNASSIA_TRINERVIS_GET_ADAPTERS_H

#include <string>
#include <unordered_map>
#include <netdb.h>
#include "base.h"

BEGIN_NAMESPACE(NET)

typedef struct {
    char description[NI_MAXHOST + 1];
    unsigned char mac_address[6];
    unsigned char ipv4_address[4];
}NetAdapterInfo;

// get all net adapter's info
FUNCTION_RETURN getAdaptersInfo(std::unordered_map<std::string, NetAdapterInfo>& adapters_info);

// get default net adapter's info
FUNCTION_RETURN getDefAdapterInfo(NetAdapterInfo& param);

// get default net interface
std::string getDefInterface();

std::string ipv4ToString(unsigned char ipv4[]);

std::string macToString(unsigned char mac_address[]);

END_NAMESPACE

#endif   // PARNASSIA_TRINERVIS_GET_ADAPTERS_H

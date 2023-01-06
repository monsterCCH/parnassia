#ifndef PARNASSIA_TRINERVIS_CONFIG_H
#define PARNASSIA_TRINERVIS_CONFIG_H
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

namespace CONFIG{

class redisCluster{
    public:
    std::string redis_server;
    std::string passwd;
    std::string auth;
    int type;

public:
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(redisCluster, redis_server, passwd, auth, type)
};

class config
{
private:
    config();
    ~config() = default;
    typedef struct {
        std::string host;
        std::string user;
        std::string pass;
    }REDIS_NODE;

    std::vector<redisCluster> _redisCluster;
    nlohmann::json _js;
    const std::string cfg_file = "/etc/parnassia.cfg";
    const std::string redis_cluster_item = "redis_cluster";
public:
    config(const config&) = delete;
    config &operator=(const config&) = delete;
    static config& instance();
    bool set_config_str(const std::string& key, const std::string& value);
    const std::vector<redisCluster>& get_redisCluster();
};

}


#endif   // PARNASSIA_TRINERVIS_CONFIG_H

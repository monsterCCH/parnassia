#include "config.h"
#include "utils_file.h"
#include "logger.h"

namespace CONFIG {



config::config()
{
    if (!file_exists(cfg_file)) {
        LOG->error("No redis connection info, config file {} isn't exist ",
                   cfg_file);
        return;
    }
    try {
        std::ifstream f(cfg_file);
        _js                          = nlohmann::json::parse(f);
        nlohmann::json redis_cluster = _js[redis_cluster_item];
        _redisCluster = redis_cluster.get<std::vector<redisCluster> >();
    }
    catch (std::exception& e) {
        LOG->error("Parse config file error : {}", e.what());
    }
}

config& config::instance()
{
    static config s_instance;
    return s_instance;
}

const std::vector<redisCluster>& config::get_redisCluster()
{
    return _redisCluster;
}

}

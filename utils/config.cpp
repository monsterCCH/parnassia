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
        _js = nlohmann::json::parse(f);
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

bool config::set_config_str(const std::string& key, const std::string& value)
{
    if (!file_exists(cfg_file)) {
        LOG->error("No redis connection info, config file {} isn't exist ",
                   cfg_file);
        return false;
    }
    try {
        std::ifstream f(cfg_file);
        _js = nlohmann::json::parse(f);
        _js[key] = value;

        std::ofstream out(cfg_file,std::ios::trunc|std::ios::out);
        if (out.is_open()) {
            out << _js.dump(4);
            out.close();
            return true;
        }
    }
    catch (std::exception& e) {
        LOG->error("Parse config file error : {}", e.what());
    }
    return false;
}

const std::vector<redisCluster>& config::get_redisCluster()
{
    return _redisCluster;
}

}

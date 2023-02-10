#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
#include <unordered_map>
#include <map>
#include <vector>
#include <mutex>
#include "ini_parser.h"
#include "redis++.h"
#include "host_info.h"
#include "nlohmann/json.hpp"

#define TYPE_REDIS_CLUSTER 1

BEGIN_NAMESPACE(CONFIG_MANAGER)


class ConfigManager : public IniParser, hostInfo
{
public:
    typedef struct FileStat {
        int64_t file_size;
        int64_t modify_time;
        int64_t create_time;
    }FileStat;

    explicit ConfigManager(const std::string& ini_file = "/etc/config_manager.ini");
    ~ConfigManager() override = default;

    void run();
    funcRes check_config(INI_MAP* ini_map, const std::string& module_name = "", const std::string& file_name = "", bool confirm = false);
    int set_config(INI_MAP* ini_map);
    int del_section(const std::vector<std::string>& secs);

private:
    void init();
    void redis_subscriber();
    funcRes task(const string& msg);
    funcRes file_opt(const string& msg);
    funcRes file_read(const nlohmann::json& js);
    funcRes file_write(const nlohmann::json& js);
    funcRes module_control(const nlohmann::json& js);
    funcRes config_setting(const nlohmann::json& js);
    funcRes control_execute(const std::string& module_name, const std::string& opt);

private:
    std::mutex mutex_;
    std::shared_ptr<sw::redis::RedisCluster> redis_cluster_;

};

END_NAMESPACE

#endif   // CONFIG_MANAGER_H

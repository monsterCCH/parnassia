#include "config_manager.h"
#include "logger.h"
#include "utils_file.h"
#include "utils_string.h"
#include "config.h"
#include "redis_cl_manager.h"
#include "nlohmann/json.hpp"
#include "Thread.h"

enum IniItem {
    II_CONFIG = 0,
    II_START,
    II_STOP,
    II_RESTART,
    II_CHECK,
    II_CONFIRM,
    II_MODIFY_TIME,
    II_FILE_SIZE
};

static std::map<int, std::string> IniItemMap = {
    {II_CONFIG, "config"},
    {II_START, "start"},
    {II_STOP, "stop"},
    {II_RESTART, "restart"},
    {II_CHECK, "check"},
    {II_CONFIRM, "confirm"},
    {II_MODIFY_TIME, "modify_time"},
    {II_FILE_SIZE, "file_size"}
};

enum TaskTypeItem {
    TT_MODUlE_CONTROL = 0,
    TT_CONFIG_SETTING
};

enum ConfManagerItem {
    CM_MODULE = 0,
    CM_CONF,
    CM_STATUS,
    CM_MTIME,
    CM_ERRMSG,
    CM_ERRCODE,
    CM_OPERATION,
    CM_ACTION,
    CM_CONTENT
};

static std::map<int, std::string> ConfManagerItemMap = {
    {CM_MODULE, "moduleName"},
    {CM_CONF, "confFile"},
    {CM_STATUS, "status"},
    {CM_MTIME, "modifyTime"},
    {CM_ERRMSG, "errMsg"},
    {CM_ERRCODE, "errCode"},
    {CM_OPERATION, "operation"},
    {CM_ACTION, "action"},
    {CM_CONTENT, "content"}
};

enum RedisChannelItem {
    RC_CONF_MONITOR = 0,
    RC_CONF_MANAGER_TASK_RET,
    RC_SMALL_FILE_TASK_RET
};

static std::map<int, std::string> RedisChannelItemMap = {
    {RC_CONF_MONITOR, "ConfMonitor"},
    {RC_CONF_MANAGER_TASK_RET, "RetConfManagerTask"},
    {RC_SMALL_FILE_TASK_RET, "RetSmallFileTask"}
};

enum RedisSublItem {
    RS_CONF_MANAGER_TASK = 0,
    RS_SMALL_FILE_TASK
};

static std::map<int, std::string> RedisSubItemMap = {
    {RS_CONF_MANAGER_TASK, "ConfManagerTask"},
    {RS_SMALL_FILE_TASK, "SmallFileTask"}
};

#define CHECK_ADD_NONE            \
    if (it == configs.begin()) {  \
        modify_time += "none";    \
        file_size += "none";      \
    }                             \
    else {                        \
        modify_time += " | none"; \
        file_size += " | none";   \
    }
#define CHECK_ADD_VALUE                                        \
    if (it == configs.begin()) {                               \
        modify_time += std::to_string(fs.modify_time);         \
        file_size += std::to_string(fs.file_size);             \
    }                                                          \
    else {                                                     \
        modify_time += " | " + std::to_string(fs.modify_time); \
        file_size += " | " + std::to_string(fs.file_size);     \
    }
using namespace CONFIG_MANAGER;

ConfigManager::ConfigManager(const std::string& ini_file) : IniParser(ini_file), mutex_()
{
    init();
    //TODO init run once ?
//    check_config(get_ini_map());
}

void ConfigManager::init()
{
    try {
        sw::redis::Optional<sw::redis::ConnectionOptions> opts;
        auto conf = CONFIG::config::instance().get_redisCluster();
        for (const auto& item : conf) {
            if (item.type == TYPE_REDIS_CLUSTER) {
                std::pair<string, int> ip_port;
                if (!redisClManager::parseServer(item.redis_server, ip_port)) {
                    LOG->warn("Config Manager: parser redis_server fail");
                    return;
                }
                opts->host     = ip_port.first;
                opts->port     = ip_port.second;
                opts->password = item.passwd;
            }
        }
        redis_cluster_ = std::make_shared<sw::redis::RedisCluster>(*opts);
        LOG->info("Config Manager connect to redis");
    }
    catch (exception& e) {
        LOG->warn("redis cluster init fail : {}", e.what());
    }
}

void ConfigManager::run()
{
    try {
        Thread trd([this] { redis_subscriber(); }, "CM:redis_sub");
        trd.start();
        //    trd.join();
    } catch (std::exception& e) {
        LOG->warn("Config Manager run fail : {}", e.what());
    } catch (...) {
        LOG->warn("Config Manager run fail : unknown error");
    }

}

funcRes ConfigManager::control_execute(const std::string& module_name, const std::string& opt)
{
    try {
        INI_MAP* iniMap = get_ini_map();
        auto module_map = (*iniMap).find(module_name);
        if (module_map == (*iniMap).end()) {
            LOG->warn("{} {} fail: no such module", module_name, opt);
            std::string err_msg = module_name + " " + opt + " fail: no such module";
            return {false, err_msg};
        }
        auto cmd_map = module_map->second.find(opt);
        if (cmd_map == module_map->second.end()) {
            LOG->warn("{} {} fail: no such command", module_name, opt);
            std::string err_msg = module_name + " " + opt + " fail: no such command";
            return {false, err_msg};
        }
        std::string cmd = cmd_map->second;
        std::string output;
        redisClManager::execute(cmd, output);
        return {true, output};

    } catch (std::exception& e) {
        LOG->warn("ConfigManager control_execute error: {}", e.what());
    }
    return {true, {}};
}

funcRes ConfigManager::file_read(const nlohmann::json& js)
{
    try {
        std::string msg_id;
        auto ele = js.find("msgId");
        if (ele != js.end()) {
            msg_id = ele.value();
        }
        std::string file_name;
        ele = js.find("fileName");
        if (ele != js.end()) {
            file_name = ele.value();
        }
        std::string content;
        int err;
        ReadSmallFile file(file_name);
        err = file.readToString(ReadSmallFile::BufSize, &content);
        nlohmann::json ret_js;
        ret_js[BaseMacroMap[BM_SYS_ID]] = getSysId();
        ret_js[BaseMacroMap[BM_MSG_ID]]  = msg_id;
        ret_js[ConfManagerItemMap[CM_ACTION]] = 0;
        ret_js[ConfManagerItemMap[CM_CONTENT]] = content;
        ret_js[ConfManagerItemMap[CM_ERRCODE]] = err? -1 : 0;
        ret_js[ConfManagerItemMap[CM_ERRMSG]] = ::strerror(err);
        redis_cluster_->publish(RedisChannelItemMap[RC_SMALL_FILE_TASK_RET], ret_js.dump());
    } catch (std::exception &e) {
        LOG->warn("ConfigManager module_control error: {}", e.what());
    }
    return {true, {}};
}

funcRes ConfigManager::file_write(const nlohmann::json& js)
{
    try {
        std::string msg_id;
        auto ele = js.find("msgId");
        if (ele != js.end()) {
            msg_id = ele.value();
        }
        std::string file_name;
        ele = js.find("fileName");
        if (ele != js.end()) {
            file_name = ele.value();
        }

        std::string content;
        ele = js.find("content");
        if (ele != js.end()) {
            content = ele.value();
        }
        bool ret = WriteFile(file_name, content, true);
        if (ret) {
            check_config(get_ini_map(), "", file_name, true);
        }

        nlohmann::json ret_js;
        ret_js[BaseMacroMap[BM_SYS_ID]] = getSysId();
        ret_js[BaseMacroMap[BM_MSG_ID]]  = msg_id;
        ret_js[ConfManagerItemMap[CM_ACTION]] = 1;
        ret_js[ConfManagerItemMap[CM_ERRCODE]] = ret? 0 : -1;
        ret_js[ConfManagerItemMap[CM_ERRMSG]] = ret? "" : "write file fail";
        redis_cluster_->publish(RedisChannelItemMap[RC_SMALL_FILE_TASK_RET], ret_js.dump());
    } catch (std::exception &e) {
        LOG->warn("ConfigManager module_control error: {}", e.what());
    }
    return {true, {}};
}

funcRes ConfigManager::module_control(const nlohmann::json& js)
{
    try {
        std::string msg_id;
        auto ele = js.find("msgId");
        if (ele != js.end()) {
            msg_id = ele.value();
        }
        nlohmann::json::object_t obj = js["taskContent"];
        std::string module_name = obj["moduleName"];
        std::string opt = obj["moduleOpt"];
        funcRes ret;
        nlohmann::json ret_js;
        ret_js[BaseMacroMap[BM_SYS_ID]] = getSysId();
        ret_js[BaseMacroMap[BM_MSG_ID]]  = msg_id;
        ret_js[ConfManagerItemMap[CM_MODULE]] = module_name;
        ret_js[ConfManagerItemMap[CM_OPERATION]] = opt;

        if (opt == IniItemMap[II_CHECK]) {
            ret = check_config(get_ini_map(), module_name);
        } else if (opt == IniItemMap[II_CONFIRM]) {
            ret = check_config(get_ini_map(), module_name, "", true);
        } else {
            ret = control_execute(module_name, opt);
        }
        ret_js[ConfManagerItemMap[CM_ERRCODE]] = ret.result? 0 : -1;
        ret_js[ConfManagerItemMap[CM_ERRMSG]] = ret.err_msg;
        redis_cluster_->publish(RedisChannelItemMap[RC_CONF_MANAGER_TASK_RET], ret_js.dump());
    } catch (std::exception &e) {
        LOG->warn("ConfigManager module_control error: {}", e.what());
    }
    return {true, {}};
}

funcRes ConfigManager::config_setting(const nlohmann::json& js)
{
    try {
        std::string err_msg;
        std::string msg_id;
        auto ele = js.find("msgId");
        if (ele != js.end()) {
            msg_id = ele.value();
        }
        nlohmann::json::object_t obj = js["taskContent"];
        std::string module_name = obj["moduleName"];
        std::string action = obj["action"];
        if (action != "del" && action != "set") {
            LOG->warn("ConfigManager config_setting error: no support action {}", action);
            err_msg = "no support action";
        }

        if (action == "del") {
            std::vector<std::string> sec{module_name};
            del_section(sec);
        }

        if (action == "set") {
            std::string config;
            nlohmann::json config_array = obj[IniItemMap[II_CONFIG]];
            for (nlohmann::json::iterator it = config_array.begin(); it != config_array.end(); ++it) {
                if (it == config_array.begin()) {
                    config += *it;
                }
                else {
                    config += " | " + (*it).get<std::string>();
                }
            }
            std::string start_cmd = obj[IniItemMap[II_START]];
            std::string stop_cmd = obj[IniItemMap[II_STOP]];
            std::string restart_cmd = obj[IniItemMap[II_RESTART]];
            ::INI_MAP new_ini;
            new_ini[module_name].insert(std::make_pair(IniItemMap[II_CONFIG], config));
            new_ini[module_name].insert(std::make_pair(IniItemMap[II_START], start_cmd));
            new_ini[module_name].insert(std::make_pair(IniItemMap[II_STOP], stop_cmd));
            new_ini[module_name].insert(std::make_pair(IniItemMap[II_RESTART], restart_cmd));
            set_config(&new_ini);
        }

        nlohmann::json ret_js;
        ret_js[BaseMacroMap[BM_SYS_ID]] = getSysId();
        ret_js[BaseMacroMap[BM_MSG_ID]]  = msg_id;
        ret_js[ConfManagerItemMap[CM_MODULE]] = module_name;
        ret_js[ConfManagerItemMap[CM_OPERATION]] = action;
        ret_js[ConfManagerItemMap[CM_ERRCODE]] = err_msg.empty()? 0 : -1;
        ret_js[ConfManagerItemMap[CM_ERRMSG]] = err_msg;
        redis_cluster_->publish(RedisChannelItemMap[RC_CONF_MANAGER_TASK_RET], ret_js.dump());

    } catch (std::exception& e) {
        LOG->warn("ConfigManager config_setting error: {}", e.what());
    }

    return {true, {}};
}

funcRes ConfigManager::task(const string& msg)
{
    try {
        LOG->debug("CongfigManager task: {}", msg);
        nlohmann::json js = nlohmann::json::parse(msg);
        int task_type = -1;
        auto obj = js.find("taskType");
        if (obj != js.end()) {
            task_type = obj.value();
        }

        switch (task_type) {
        case TT_MODUlE_CONTROL:
            module_control(js);
            break;
        case TT_CONFIG_SETTING:
            config_setting(js);
            break;
        default:
            LOG->warn("ConfigManager task: invalid task type {0:d}", task_type);
        }
    } catch (std::exception& e) {
        LOG->warn("ConfigManager task error: {}", e.what());
    }

    return {true, {}};
}

void ConfigManager::redis_subscriber()
{
    for (;;) {

        try {
            auto sub = redis_cluster_->subscriber();
            std::unordered_set<std::string> channels;
            sub.on_meta([&channels](sw::redis::Subscriber::MsgType type,
                                    sw::redis::OptionalString      channel,
                                    long long                      nums) {
                if (type == sw::redis::Subscriber::MsgType::SUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    channels.insert(*channel);
                }
                else if (type == sw::redis::Subscriber::MsgType::UNSUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    channels.erase(*channel);
                }
                else {
                    LOG->error("Unknown message type");
                }
            });
            sub.on_message([this](std::string channel, std::string msg) {
                if (channel == RedisSubItemMap[RS_CONF_MANAGER_TASK]) {
                    funcRes ret = task(msg);
                }

                if (channel == RedisSubItemMap[RS_SMALL_FILE_TASK]) {
                    funcRes ret = file_opt(msg);
                }
            });
            sub.subscribe({RedisSubItemMap[RS_CONF_MANAGER_TASK], RedisSubItemMap[RS_SMALL_FILE_TASK]});
            for (std::size_t idx = 0; idx != channels.size(); ++idx) {
                sub.consume();
            }

            for (;;) {
                sub.consume();
                usleep(100000);
            }
        }
        catch (std::exception& e) {
            LOG->warn("Config Manager: redis_subscriber fail, {}", e.what());
        }
    }

}

funcRes ConfigManager::file_opt(const string& msg)
{
    try {
        LOG->debug("CongfigManager task: {}", msg);
        nlohmann::json js = nlohmann::json::parse(msg);
        int action = -1;
        auto obj = js.find("action");
        if (obj != js.end()) {
            action = obj.value();
        }

        switch (action) {
        case 0: //read
            file_read(js);
            break;
        case 1: //write
            file_write(js);
            break;
        default:
            LOG->warn("ConfigManager action: invalid action {0:d}", action);
        }
    } catch (std::exception& e) {
        LOG->warn("Config Manager: file_opt fail: {}.", e.what());
    }
    return {true, {}};
}

funcRes ConfigManager::check_config(INI_MAP* ini_map, const std::string& module_name, const std::string& file_name, bool confirm)
{
    std::string err_msg;
    try {
        std::lock_guard lck(mutex_);
        for (auto& iter : (*ini_map)) {
            bool isReload = false;
            std::string module = iter.first;
            if (!module_name.empty() && module != module_name) continue ;

            auto conf_item = iter.second.find(IniItemMap[II_CONFIG]);
            if (conf_item == iter.second.end()) continue ;

            std::string config = conf_item->second;
            if (config.empty()) {
                LOG->info("Config Manager: Module [{}] have no config path", module);
                if (!module_name.empty()) {
                    err_msg += module + " don't have config file path.";
                }
                continue ;
            }

            std::vector<std::string> configs;
            split(config, configs, "|");
            std::vector<std::string> old_sizes;
            std::vector<std::string> old_modifys;

            auto size_item = iter.second.find(IniItemMap[II_FILE_SIZE]);
            if (size_item != iter.second.end()) {
                std::string size = size_item->second;
                split(size, old_sizes, "|");
            }

            auto modify_item = iter.second.find(IniItemMap[II_MODIFY_TIME]);
            if (modify_item != iter.second.end()) {
                std::string modify = modify_item->second;
                split(modify, old_modifys, "|");
            }

            if (old_sizes.size() != configs.size() || old_modifys.size() != configs.size()) {
                LOG->info("Config Manager: Module [{}] config has updated, reload.", module);
                isReload = true;
                std::vector<std::string>().swap(old_sizes);
                std::vector<std::string>().swap(old_modifys);
                iter.second[IniItemMap[II_FILE_SIZE]] = "";
                iter.second[IniItemMap[II_MODIFY_TIME]] = "";
            }


            std::string modify_time;
            std::string file_size;
            int i = 0;
            for (std::vector<std::string>::iterator it = configs.begin(); it != configs.end(); it++, i++) {
                std::unique_ptr<ReadSmallFile> file{new ReadSmallFile(trim_copy(*it))};
                if (file->getErr() != 0) {
                    LOG->warn("Config Manager: Module [{}] open file [{}] fail.", module, *it);
                    if (!module_name.empty()) {
                        err_msg += module + " open config file " + *it + " fail.";
                    }
                    CHECK_ADD_NONE;
                    continue ;
                }

                FileStat fs{0};
                if (file->readFileStat(&fs.file_size, &fs.modify_time, &fs.create_time) != 0) {
                    LOG->warn("Config Manager: Module [{}] read conf file stat [{}] fail.", module, *it);
                    if (!module_name.empty()) {
                        err_msg += module + " read config file " + *it + " stat fail.";
                    }
                    CHECK_ADD_NONE;
                    continue ;
                }

                if (isReload) {
                    CHECK_ADD_VALUE;
                } else {
                    try {
                        bool temp_confirm = confirm;
                        if (std::to_string(fs.modify_time) != trim_copy(old_modifys[i])
                            || std::to_string(fs.file_size) != trim_copy(old_sizes[i])) {
                            LOG->warn("Config Manager: Module [{}] conf file [{}] has been modify unexpectedly.", module, *it);

                            if (trim_copy(old_modifys[i]) == "none"
                                && trim_copy(old_sizes[i]) == "none") {
                                confirm = true;
                            }

                            if (!file_name.empty() && file_name == (*it)) {
                                confirm = true;
                            }

                            if (it == configs.begin()) {
                                modify_time += confirm? std::to_string(fs.modify_time) : trim_copy(old_modifys[i]);
                                file_size += confirm? std::to_string(fs.file_size) : trim_copy(old_sizes[i]);
                            }
                            else {
                                modify_time += " | " + (confirm? std::to_string(fs.modify_time) : trim_copy(old_modifys[i]));
                                file_size += " | " + (confirm? std::to_string(fs.file_size) : trim_copy(old_sizes[i]));
                            }
                            if (!confirm) {
                                nlohmann::json js;
                                js[BaseMacroMap[BM_SYS_ID]] = getSysId();
                                js[ConfManagerItemMap[CM_MODULE]] = module;
                                js[ConfManagerItemMap[CM_CONF]] = *it;
                                js[ConfManagerItemMap[CM_STATUS]] = "modify";
                                js[ConfManagerItemMap[CM_MTIME]] = std::to_string(fs.modify_time);

                                redis_cluster_->publish(RedisChannelItemMap[RC_CONF_MONITOR], js.dump());
                            }
                        } else {
                            LOG->info("Config Manager: Module [{}] conf file [{}] everything is ok.", module, *it);
                            CHECK_ADD_VALUE;
                        }
                        confirm = temp_confirm;
                    } catch (std::exception &e) {
                        LOG->warn("Config Manager: Module [{}] conf file [{}] have err [{}]", module, *it, e.what());
                        CHECK_ADD_NONE;
                    }
                }
            }
            iter.second[IniItemMap[II_FILE_SIZE]] = file_size;
            iter.second[IniItemMap[II_MODIFY_TIME]] = modify_time;
        }
        ini_dump();

    } catch (std::exception& e) {
        LOG->warn("Config Manager: check fail: {}.", e.what());
    }

    return {err_msg.empty(), err_msg};
}

int ConfigManager::set_config(INI_MAP* net_ini)
{
    INI_MAP *ini_map = get_ini_map();
    try {
        std::lock_guard lck(mutex_);
        for (const auto& iter : (*net_ini)) {
            (*ini_map)[iter.first] = iter.second;
        }
        ini_dump();
    } catch (std::exception& e) {
        LOG->warn("Config Manager: set config err. {}", e.what());
        return -1;
    }
    check_config(ini_map);
    return 0;
}

int ConfigManager::del_section(const std::vector<std::string>& secs)
{
    INI_MAP *ini_map = get_ini_map();
    try {
        std::lock_guard lck(mutex_);
        for (const auto& iter : secs) {
            auto it = (*ini_map).find(iter);
            if (it != (*ini_map).end()) {
                it = (*ini_map).erase(it);
            }
        }
        ini_dump();
    } catch (std::exception& e) {
        LOG->warn("Config Manager: del section err. {}", e.what());
    }
}

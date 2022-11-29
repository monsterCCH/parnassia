#include "redis_cl_manager.h"
#include "logger.h"
#include "utils_string.h"
#include <hiredis/hiredis.h>
#include "get_adapters.h"
#include "nlohmann/json.hpp"
#include "timer.h"
#include "host_info.h"
#include "scp_file.h"
using namespace std;

#define IP_PORT_SEPARATOR ':'

std::map<int, std::string> redisClManager::TopicMap = {
    {static_cast<int>(TOPICS::COMMAND_INFO), "CommandInfo"},
    {static_cast<int>(TOPICS::DELIVER_FILE), "DeliverFile"}
};

string redisClManager::host_ip = {};
string redisClManager::host_id = {};
std::mutex redisClManager::deliver_mutex {};

redisClManager::redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout) : tv(timeout) , thread_pool(4)
{
    host_ip = m_hi.getIpAddr();
    host_id = m_hi.getSysId();
    redisSubInit();
    for (const auto& iter : redis_info) {
        if (iter.type == 1) {
            redisCLInit(iter);
        }

        if (iter.type == 0) {
            redisInit(iter);
        }
    }
    redisCLSubscriber();
}

redisClManager::~redisClManager()
{
    for (const auto& iter : vec_redisAsyncSubscriber) {
        iter->disconnect();
        iter->uninit();
    }

    for (const auto& iter : vec_redisAsyncPublish) {
        iter->disconnect();
        iter->uninit();
    }
}

void redisClManager::redisPublish(const std::string& command,
                                    const std::string& json)
{
    try {
        for (const auto& iter : vec_rRedisCl) {
            iter->publish(command, json);
        }
    } catch (exception& e) {
        LOG->warn("{} publish fail", command);
    }


    for (auto& iter : vec_redisAsyncPublish) {
        iter->publish(command, json);
    }

}

void redisClManager::redisCLInit(const CONFIG::redisCluster& rc)
{
#if 0
    redisClusterContext* cc = redisClusterContextInit();
    redisClusterSetOptionAddNodes(cc, rc.redis_server.c_str());
    redisClusterSetOptionConnectTimeout(cc, tv);
    if (!rc.auth.empty()) {
        redisClusterSetOptionUsername(cc, rc.auth.c_str());
    }
    if (!rc.passwd.empty()) {
        redisClusterSetOptionPassword(cc, rc.passwd.c_str());
    }
    redisClusterConnect2(cc);
    if (cc && cc->err) {
        LOG->warn("Connect to {} fail, error : {}",
                  rc.redis_server,
                  cc->errstr);
        return;
    }
    vec_redisClContext.emplace_back(cc);
#endif
    try {
        std::pair<string, int> ip_port;
        if (!parseServer(rc.redis_server, ip_port)) {
            return ;
        }
        sw::redis::ConnectionOptions tmp;
        tmp.host = ip_port.first;
        tmp.port = ip_port.second;
        tmp.password = rc.passwd;
        sw::redis::Optional<sw::redis::ConnectionOptions> opts;
        opts = sw::redis::Optional<sw::redis::ConnectionOptions>(tmp);
        std::shared_ptr<sw::redis::RedisCluster> rcp = std::make_shared<sw::redis::RedisCluster>(*opts);
        LOG->info("Connect to redis {}", rc.redis_server);
        vec_rRedisCl.emplace_back(rcp);
    } catch (exception& e) {
        LOG->warn("redis cluster {} init fail : {}", rc.redis_server, e.what());
    }

}

void redisClManager::redisInit(const CONFIG::redisCluster& rc) 
{
    std::pair<string, int> ip_port;
    if (!parseServer(rc.redis_server, ip_port)) {
        return ;
    }
    LOG->info("Connect to redis {}", rc.redis_server);
    std::shared_ptr<redisAsyncOpt> rp = std::make_shared<redisAsyncOpt>(rc.passwd, ip_port.first, ip_port.second);
    bool ret = rp->create();
    if (!ret) {
        LOG->warn("redis {}:{0:d} create failed", ip_port.first, ip_port.second);
        return;
    }
    vec_redisAsyncPublish.emplace_back(rp);

    rp = std::make_shared<redisAsyncOpt>(rc.passwd, ip_port.first, ip_port.second);
    ret = rp->create();;
    if (!ret) {
        LOG->warn("redis {}:{0:d} create failed", ip_port.first, ip_port.second);
        return;
    }
    ret = rp->set_subscriber(vec_topic);
    if (!ret) {
        LOG->warn("redis {}:{0:d} set_subscriber failed", ip_port.first, ip_port.second);
        return;
    }
    vec_redisAsyncSubscriber.push_back(rp);
}

bool redisClManager::parseServer(const string& server, std::pair<string, int>& ip_port)
{
    char *p;
    char *addr = const_cast<char *>(server.c_str());
    if ((p = strrchr(addr, IP_PORT_SEPARATOR)) == NULL) {
        LOG->warn("server address is incorrect, port separator missing");
        return false;
    }

    if (p - addr <= 0) {
        LOG->warn("server address is incorrect, address part missing");
        return false;
    }
    ip_port.first = server.substr(0, p - addr);
    p++;

    if (strlen(p) <= 0) {
        LOG->warn("server address is incorrect, port part missing");
        return false;
    }

    ip_port.second = hi_atoi((uint8_t *)p, strlen(p));
    if (ip_port.second <= 0) {
        LOG->warn("server port is incorrect");
        return false;
    }
    return true;
}

void redisClManager::redisSubInit()
{
    for (const auto& iter : TopicMap) {
        vec_topic.push_back(iter.second);
    }
}

void redisClManager::redisCLSubscriber()
{
    for (const auto& iter : vec_rRedisCl) {
        for (const auto& it : vec_topic) {
            auto * param = new SubParam();
            param->hostIp = host_ip;
            param->topic = it;
            param->instance = iter;
            param->selfPtr = this;
            pthread_t t = ThreadCreate(redisCLsub, param);
            pthread_detach(t);
        }
    }
}

int redisClManager::getTopicId(const std::string& topic)
{
    for (const auto& iter : TopicMap) {
        if (iter.second == topic) {
            return iter.first;
        }
    }
    return -1;
}

[[noreturn]] void* redisClManager::redisCLsub(void* param)
{
    for(;;) {
        try {
            auto* pa  = (SubParam*) param;
            auto  sub = pa->instance->subscriber();
            sub.on_message([&pa](const string& channel, const string& msg) {
                int topic_id = getTopicId(pa->topic);
                switch (topic_id) {
                case static_cast<int>(TOPICS::COMMAND_INFO):
                    exeCommand(pa, msg);
                    break ;
                case static_cast<int>(TOPICS::DELIVER_FILE):
                    deliverFile(pa, msg);
                    break;
                default:
                    break;
                }
            });

            sub.subscribe(pa->topic);
            for (;;) {
                sub.consume();
                usleep(1000);
            }
        } catch (exception& e) {
            LOG->warn("redis_cluster subscriber error : {}", e.what());
        }
    }
}

void redisClManager::deliverFile(void* param, const std::string& msg)
{
    std::lock_guard<std::mutex> lock(deliver_mutex);
    auto *pa = (SubParam*) param;
    try {
        nlohmann::json js = nlohmann::json::parse(msg);
        string m_host_id;
        std::string msg_id;
        auto obj = js.find("robotId");
        if (obj != js.end()) {
            m_host_id = obj.value();
        }
        obj = js.find("msgId");
        if (obj != js.end()) {
            msg_id = obj.value();
        }

        if (m_host_id == redisClManager::getHostId()) {
            int direction = js["direction"].get<int>();
            nlohmann::json host_info_array = js["hostInfo"];
            nlohmann::json file_info_array = js["params"];
            std::vector<SCP::ScpFile::hostInfo> host_info_vec;
            std::vector<fileInfo> file_info_vec;
            SCP::ScpFile::hostInfo src_host, dst_host;

            file_info_vec = file_info_array.get<vector<fileInfo>>();
            host_info_vec = host_info_array.get<vector<SCP::ScpFile::hostInfo>>();


            for (auto & iter : host_info_vec) {
                if (iter.type == 0) src_host = iter;
                if (iter.type == 1) dst_host = iter;
            }

            std::unique_ptr<SCP::ScpFile> scp;
            if (direction == 1 && !src_host.hostIp.empty() && src_host.hostIp != redisClManager::getHostIp()) {
                scp = make_unique<SCP::ScpFile>(src_host);
            } else if (direction == 2 && !src_host.hostIp.empty() && !dst_host.hostIp.empty()
                     && src_host.hostIp != redisClManager::getHostIp() && dst_host.hostIp != redisClManager::getHostIp()){
                scp = make_unique<SCP::ScpFile>(src_host, dst_host);
            } else {
                LOG->warn("hostInfo or direction is invalid");
            }
            if (scp == nullptr || !scp->getInitRet()) {
                throw std::runtime_error("SCP init fail");
            }
            nlohmann::json ret_js;
            ret_js["robotId"] = m_host_id;
            ret_js["msgId"] = msg_id;
            nlohmann::json result_array = nlohmann::json::array();

            for (const auto& iter : file_info_vec) {
                    std::vector<int> ret = scp->transFile(iter.fileName, iter.srcFilePath, iter.dstFilePath);

                    nlohmann::json js_obj = nlohmann::json::object();
                    js_obj.push_back({"srcFilePath", iter.srcFilePath});
                    nlohmann::json res_obj = nlohmann::json::object();
                    vector<string> files = iter.fileName;
                    auto first = files.begin();
                    for (auto it = files.begin(); it != files.end(); ++it) {
                        res_obj.push_back({*it, ret[distance(first, it)]});
                    }
                    js_obj.push_back({"fileName", res_obj});
                    result_array.push_back(js_obj);
            }
            ret_js["result"] = result_array;
            std::string ret_str = ret_js.dump();

            pa->selfPtr->redisPublish("DeliverFileRet", ret_str);
        }

    }
    catch (std::runtime_error& e) {
        LOG->warn("runtime_error {} : {}", msg, e.what());
    }
    catch (exception& e) {
        LOG->warn("Invalid json string {} : {}", msg, e.what());
    }

}

void redisClManager::exeCommand(void *param, const std::string& msg)
{
    auto* pa = (SubParam*) param;
    try {
        nlohmann::json js = nlohmann::json::parse(msg);
        string m_host_id;
        std::string msg_id;
        int type  = 0;
        auto obj = js.find("robotId");
        if (obj != js.end()) {
            m_host_id = obj.value();
        }
        obj = js.find("msgId");
        if (obj != js.end()) {
            msg_id = obj.value();
        }
        obj = js.find("type");
        if (obj != js.end()) {
            type = obj.value();
        }

        if (m_host_id == redisClManager::getHostId()) {
            vector<string> command = js["command"];
            std::vector<std::string> results;
            std::unique_ptr<SCP::ScpFile> scp;
            if (type) {
                nlohmann::json host_info_obj = js["hostInfo"];
                sshInfo hi = host_info_obj.get<sshInfo>();
                SCP::ScpFile::hostInfo dst_host {.hostIp=hi.hostIp, .user=hi.user, .passwd=hi.passwd, .type=1, .port=hi.port};
                if (!dst_host.hostIp.empty() && dst_host.hostIp != redisClManager::getHostIp()) {
                    scp = make_unique<SCP::ScpFile>(dst_host);
                }
                if (scp == nullptr || !scp->getInitRet()) {
                    throw std::runtime_error("SCP init fail");
                }
            }

            results.reserve(command.size());
            for (const auto& iter : command) {
                if (!type) { // local execute
                    string res;
                    execute(iter, res);
                    results.emplace_back(res);
                }else { // remote execute
                    string res;
                    scp->execute(scp->getSrcHostInfo(), iter, res);
                    results.emplace_back(res);
                }
            }

            nlohmann::json ret_js;
            ret_js["robotId"] = m_host_id;
            ret_js["msgId"] = msg_id;
            nlohmann::json array = nlohmann::json::array();
            for (auto& it : results) {
                array.push_back(it);
            }
            ret_js["errMsg"] = array;
            std::string ret_str = ret_js.dump();

            pa->selfPtr->redisPublish("CommandInfoRet", ret_str);
        }
    }
    catch (std::runtime_error& e) {
        LOG->warn("runtime_error {} : {}", msg, e.what());
    }
    catch (exception& e) {
        LOG->warn("Invalid json string {} : {}", msg, e.what());
    }
}

int redisClManager::execute(std::string cmd, string& output)
{
    const int size = 128;
    std::array<char, size> buffer{};

    auto pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    size_t count;
    do {
        if ((count = fread(buffer.data(), 1, size, pipe)) > 0) {
            output.insert(output.end(),
                          std::begin(buffer),
                          std::next(std::begin(buffer), count));
        }
    } while (count > 0);

    return pclose(pipe);
}

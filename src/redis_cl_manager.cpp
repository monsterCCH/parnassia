#include "redis_cl_manager.h"
#include "logger.h"
#include "utils_string.h"
#include <hiredis/hiredis.h>
#include "get_adapters.h"
#include "nlohmann/json.hpp"
#include "timer.h"
using namespace std;

#define IP_PORT_SEPARATOR ':'

std::map<int, std::string> redisClManager::TopicMap = {
    {static_cast<int>(TOPICS::DOCKER_COMMAND), "DockerInstructions"}
};

string redisClManager::host_ip = {};

redisClManager::redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout) : tv(timeout)
{
    NET::NetAdapterInfo adapter_info;
    if (getDefAdapterInfo(adapter_info) == FUNC_RET_OK) {
        host_ip = NET::ipv4ToString(adapter_info.ipv4_address);
    }
    redisSubInit();
    for (const auto& iter : redis_info) {
        if (iter.type == 1) {
            redisCLInit(iter);
            redisCLSubscriber();
        }

        if (iter.type == 0) {
            redisInit(iter);
        }
    }
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
    std::shared_ptr<redisAsyncOpt> rp = std::make_shared<redisAsyncOpt>(ip_port.first, ip_port.second);
    bool ret = rp->create();
    if (!ret) {
        LOG->warn("redis {}:{0:d} create failed", ip_port.first, ip_port.second);
        return;
    }
    vec_redisAsyncPublish.emplace_back(rp);

    rp = std::make_shared<redisAsyncOpt>(ip_port.first, ip_port.second);
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
            pthread_t t = ThreadCreate(redisCLsub, param);
            pthread_detach(t);
        }
    }
}
[[noreturn]] void* redisClManager::redisCLsub(void* param)
{
    for(;;) {
        try {
            auto* pa  = (SubParam*) param;
            auto  sub = pa->instance->subscriber();
            sub.on_message([&pa](const string& channel, const string& msg) {
                try {
                    nlohmann::json js = nlohmann::json::parse(msg);
                    string host_id;
                    auto obj = js.find("hostId");
                    if (obj != js.end()) {
                        host_id = obj.value();
                    }

                    if (host_id == pa->hostIp) {
                        vector<string> command = js["command"];
                        for (const auto& iter : command) {
                            string res;
                            execute(iter, res);
                        }
                    }
                }
                catch (exception& e) {
                    LOG->warn("Invalid json string {} : {}", msg, e.what());
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

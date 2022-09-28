#include "redis_cl_manager.h"
#include "logger.h"
#include "utils_string.h"
#include <hiredis/hiredis.h>



#define IP_PORT_SEPARATOR ':'

redisClManager::redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout) : tv(timeout)
{
    for (const auto& iter : redis_info) {
        if (iter.type == 1) {
            redisCLInit(iter);
        }

        if (iter.type == 0) {
            redisInit(iter);
        }
    }
}

redisClManager::~redisClManager()
{
    for (const auto& iter : vec_rRedis) {

    }

    for (const auto& iter : vec_redisPub) {
        iter->disconnect();
        iter->uninit();
    }
}

void redisClManager::redisPublish(const std::string& command,
                                    const std::string& json)
{
    try {
        for (const auto& iter : vec_rRedis) {
            iter->publish(command, json);
        }
    } catch (exception& e) {
        LOG->warn("{} publish fail", command);
    }


    for (auto& iter : vec_redisPub) {
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
        vec_rRedis.emplace_back(rcp);
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
    bool ret = rp->init();
    if (!ret) {
        LOG->warn("redis {}:{0:d} init failed", ip_port.first, ip_port.second);
        return;
    }
    LOG->info("Connect to redis {}:{0:d}", ip_port.first, ip_port.second);
    ret = rp->connect();
    if (!ret) {
        LOG->warn("redis {}:{0:d} init failed", ip_port.first, ip_port.second);
        return;
    }
    vec_redisPub.emplace_back(rp); 
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

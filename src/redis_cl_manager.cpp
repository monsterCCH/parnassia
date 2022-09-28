#include <event2/event.h>
#include "redis_cl_manager.h"
#include "hiredis_cluster/hircluster.h"
#include "logger.h"
#include "utils_string.h"
#include <hiredis_cluster/adapters/libevent.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>


#define IP_PORT_SEPARATOR ':'

void pubCallback(redisAsyncContext *c, void *r, void *privdata) {

    redisReply *reply = (redisReply*)r;
    if (reply == NULL){
        LOG->warn("Response not recev");
        return;
    }
    LOG->info("{} message published", (char *)privdata);
//    redisAsyncDisconnect(c);
}

redisClManager::redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout)
{
    for (const auto& iter : redis_info) {
        if (iter.type == 1) {
            redisClusterContext* cc = redisClusterContextInit();
            redisClusterSetOptionAddNodes(cc, iter.redis_server.c_str());
            redisClusterSetOptionConnectTimeout(cc, timeout);
            if (!iter.auth.empty()) {
                redisClusterSetOptionUsername(cc, iter.auth.c_str());
            }
            if (!iter.passwd.empty()) {
                redisClusterSetOptionPassword(cc, iter.passwd.c_str());
            }
            redisClusterConnect2(cc);
            if (cc && cc->err) {
                LOG->warn("Connect to {} fail, error : {}",
                          iter.redis_server,
                          cc->errstr);
                continue;
            }
            vec_redisClContext.emplace_back(cc);
        }

        if (iter.type == 0) {
            char *p;
            char *addr = const_cast<char *>(iter.redis_server.c_str());
            if ((p = strrchr(addr, IP_PORT_SEPARATOR)) == NULL) {
                LOG->warn("server address is incorrect, port separator missing");
                continue ;
            }

            if (p - addr <= 0) { /* length until separator */
                LOG->warn("server address is incorrect, address part missing");
                continue ;
            }
            string ip = iter.redis_server.substr(0, p - addr);
            p++; // remove separator character

            if (strlen(p) <= 0) {
                LOG->warn("server address is incorrect, port part missing");
                continue ;
            }

            int port = hi_atoi((uint8_t *)p, strlen(p));
            if (port <= 0) {
                LOG->warn("server port is incorrect");
                continue ;
            }
            redisAsyncContext* _redisContext = redisAsyncConnect(ip.c_str(), port);
            if (!_redisContext->err) {
                vec_redisAsynContext.emplace_back(_redisContext);
            }

        }
    }
}

redisClManager::~redisClManager()
{
    for (auto iter : vec_redisClContext) {
        redisClusterFree(iter);
    }
}

void redisClManager::redisClCommand(const std::string& cmd,
                                    const std::string& json)
{
//    for (auto& iter : vec_redisClContext) {
//        redisReply *reply = (redisReply *)redisClusterCommand(iter, "PUBSUB hw_info %s", json.c_str());
//        if (!reply || iter->err) {
//            LOG->warn("redisClusterCommand error : {}",iter->errstr);
//        }
//    }

    for (auto& iter : vec_redisAsynContext) {
        int status;
        struct event_base *base = event_base_new();
        redisLibeventAttach(iter, base);
        string command ("publish ");
        command.append("hw_info ");
        command.append(json);

        status = redisAsyncCommand(iter,
                                   pubCallback,
                                   (char*)"hw_info", command.c_str());
//        event_base_dispatch(base);
    }

}

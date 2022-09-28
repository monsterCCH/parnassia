#ifndef PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#define PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#include <vector>
//#include "hiredis_cluster/hircluster.h"
#include "hiredis/hiredis.h"
#include "config.h"
#include "redis_async_opt.h"
#include "redis++.h"

using namespace std;
class redisClManager
{
private:
//    vector<redisClusterContext *> vec_redisClContext;
    vector<std::shared_ptr<sw::redis::RedisCluster> > vec_rRedis;
    vector<std::shared_ptr<redisAsyncOpt> > vec_redisPub;
    struct timeval tv;

public:
    explicit redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout = {1, 500000});
    ~redisClManager();
    void redisPublish(const std::string& command, const std::string& json);

private:
    bool parseServer(const string& server, std::pair<string, int>& ip_port);
    void redisCLInit(const CONFIG::redisCluster& rc);
    void redisInit(const CONFIG::redisCluster& rc);


};

#endif   // PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H

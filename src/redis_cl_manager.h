#ifndef PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#define PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#include <vector>
#include "hiredis_cluster/hircluster.h"
#include "hiredis/hiredis.h"
#include "config.h"

using namespace std;
class redisClManager
{
private:
    vector<redisClusterContext *> vec_redisClContext;
    vector<redisAsyncContext *> vec_redisAsynContext;
public:
    explicit redisClManager(const vector<CONFIG::redisCluster>& redis_info, struct timeval timeout = {1, 500000});
    ~redisClManager();
    void redisClCommand(const std::string& command, const std::string& json);

};

#endif   // PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H

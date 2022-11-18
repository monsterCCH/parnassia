#ifndef PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#define PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#include <vector>
#include "hiredis/hiredis.h"
#include "config.h"
#include "redis_async_opt.h"
#include "redis++.h"
#include "thread_pool.h"

class redisClManager
{
    enum class TOPICS {
        COMMAND_INFO = 0,
        DELIVER_FILE = 1
    };

    typedef struct sshInfo{
        std::string hostIp;
        std::string user;
        std::string passwd;
        uint port = 22;
    }sshInfo;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(sshInfo, hostIp, user, passwd, port)

    static std::map<int, std::string> TopicMap;
    static std::string host_ip;
    static std::string host_id;
    static std::mutex deliver_mutex;

private:
    typedef struct {
        std::string hostIp;
        std::string hostId;
        std::string topic;
        redisClManager * selfPtr;
        std::shared_ptr<sw::redis::RedisCluster> instance;
    } SubParam;
    std::vector<std::shared_ptr<sw::redis::RedisCluster> > vec_rRedisCl;
    std::vector<std::shared_ptr<redisAsyncOpt> > vec_redisAsyncPublish;
    std::vector<std::shared_ptr<redisAsyncOpt> > vec_redisAsyncSubscriber;
    std::vector<std::string> vec_topic;
    struct timeval tv;

public:
    explicit redisClManager(const std::vector<CONFIG::redisCluster>& redis_info, struct timeval timeout = {1, 500000});
    ~redisClManager();
    void redisPublish(const std::string& command, const std::string& json);
    static int execute(std::string cmd, std::string& output);
    static std::string getHostIp() { return host_ip; }
    static std::string getHostId() { return host_id; }
    typedef struct fileInfo {
        std::string srcFilePath;
        std::string dstFilePath;
        std::vector<std::string> fileName;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(fileInfo, srcFilePath, dstFilePath, fileName)
    }fileInfo;

    [[maybe_unused]]ThreadPool thread_pool;
private:
    bool parseServer(const std::string& server, std::pair<std::string, int>& ip_port);
    void redisCLInit(const CONFIG::redisCluster& rc);
    void redisInit(const CONFIG::redisCluster& rc);
    void redisSubInit();
    void redisCLSubscriber();
    static int getTopicId(const std::string& topic);
    [[noreturn]] static void* redisCLsub(void *param);
    static void exeCommand(void *param, const std::string& msg);
    static void deliverFile(void *param, const std::string& msg);
};



#endif   // PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H

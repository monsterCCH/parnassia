#ifndef PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#define PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H
#include <vector>
#include <workflow/Workflow.h>
#include <workflow/HttpMessage.h>
#include "hiredis/hiredis.h"
#include "config.h"
#include "redis_async_opt.h"
#include "redis++.h"
#include "thread_pool.h"
#include "host_info.h"

class redisClManager
{
    typedef struct {
        std::string hostIp;
        std::string hostId;
        std::string topic;
        redisClManager * selfPtr;
        std::shared_ptr<sw::redis::RedisCluster> instance;
    } SubParam;

    enum class TOPICS {
        COMMAND_INFO = 0,
        DELIVER_FILE = 1
    };

    enum DELIVER_TYPE {
        DELIVER_SSH = 1,
        DELIVER_HTTP = 2
    };

    struct httpSeriesContext {
        std::string url;
        std::string path;
        std::string file_name;
        std::string msg_id;
        SubParam* pa;
        int state;
        int error;
        protocol::HttpResponse resp;
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
    std::vector<std::shared_ptr<sw::redis::RedisCluster> > vec_rRedisCl;
    std::vector<std::shared_ptr<redisAsyncOpt> > vec_redisAsyncPublish;
    std::vector<std::shared_ptr<redisAsyncOpt> > vec_redisAsyncSubscriber;
    std::vector<std::string> vec_topic;
    struct timeval tv;
    const hostInfo m_hi;

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
    static void sshDeliver(nlohmann::json& js, SubParam* pa);
    static void httpDeliver(nlohmann::json& js, SubParam* pa);
    static void httpCallback(const ParallelWork *pwork);
};



#endif   // PARNASSIA_TRINERVIS_REDIS_CL_MANAGER_H

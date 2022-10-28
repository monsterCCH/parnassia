#ifndef REDIS_PUBLISHER_H
#define REDIS_PUBLISHER_H

#include <stdlib.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

class redisAsyncOpt
{
public:
    explicit redisAsyncOpt(std::string pass, std::string ip = "127.0.0.1", int port = 6379);
    ~redisAsyncOpt() = default;

    bool create();
    bool init();
    bool uninit();
    bool connect();
    bool set_subscriber(std::vector<std::string>& topics);
    bool disconnect();
    
    bool publish(const std::string &channel_name, 
                 const std::string &message);

private:
    // 连接回调
    static void connect_callback(const redisAsyncContext *redis_context,
                                 int status);

    // 认证回调
    static void auth_callback(redisAsyncContext *redis_context,
                              void *reply, void *privdata);
	
    // 断开连接的回调
    static void disconnect_callback(const redisAsyncContext *redis_context,
                                    int status);

    // 执行命令回调
    static void command_callback(redisAsyncContext *redis_context,
                                 void *reply, void *privdata);
    // 订阅消息回调
    static void subscriber_callback(redisAsyncContext *redis_context,
                                 void *reply, void *privdata);

    // 事件分发线程函数
    static void *event_thread(void *data);
    void *event_proc();

private:
    // libevent事件对象
    event_base *_event_base;
    // 事件线程ID
    pthread_t _event_thread;
    // 事件线程的信号量
    sem_t _event_sem{};
    // hiredis异步对象
    redisAsyncContext *_redis_context;

    std::string _ip;
    int _port;
    std::string _pass;
};

#endif

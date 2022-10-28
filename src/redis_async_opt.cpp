#include "redis_async_opt.h"
#include <cstddef>
#include <cassert>
#include <cstring>
#include <utility>
#include "logger.h"
#include "nlohmann/json.hpp"
#include "redis_cl_manager.h"

redisAsyncOpt::redisAsyncOpt(std::string pass, std::string ip, int port)
    :_event_base(nullptr), _event_thread(0), _redis_context(nullptr), _ip(std::move(ip)), _port(port), _pass(std::move(pass))
{
}

bool redisAsyncOpt::init()
{
    // initialize the event
    _event_base = event_base_new();    // 创建libevent对象
    if (NULL == _event_base) {
        LOG->warn("Create redis event failed");
        return false;
    }

    memset(&_event_sem, 0, sizeof(_event_sem));
    int ret = sem_init(&_event_sem, 0, 0);
    if (ret != 0) {
        LOG->warn("Init sem failed");
        return false;
    }

    return true;
}

bool redisAsyncOpt::uninit()
{
    _event_base = NULL;

    sem_destroy(&_event_sem);
    return true;
}

bool redisAsyncOpt::connect()
{
    // connect redis
    _redis_context = redisAsyncConnect(_ip.c_str(), _port);    // 异步连接到redis服务器上，使用默认端口
    if (NULL == _redis_context) {
        LOG->warn("Connect redis {}:{0:d} failed", _ip, _port);
        return false;
    }

    if (_redis_context->err) {
        LOG->warn("Connect redis {}:{0:d} error: {0:d}, {}", _ip, _port,  _redis_context->err, _redis_context->errstr);
        return false;
    }

    if (!_pass.empty()) {
        int ret = redisAsyncCommand(_redis_context,
                                    &redisAsyncOpt::auth_callback, this, "AUTH %s",
                                    _pass.c_str());
        if (REDIS_ERR == ret) {
            LOG->warn("Publish command failed: {0:d}", ret);
            return false;
        }
    }

    // attach the event
    redisLibeventAttach(_redis_context, _event_base);    // 将事件绑定到redis context上，使设置给redis的回调跟事件关联

    // 创建事件处理线程
    int ret = pthread_create(&_event_thread, 0, &redisAsyncOpt::event_thread, this);
    if (ret != 0) {
        LOG->warn("create event thread failed");
        disconnect();
        return false;
    }

    // 设置连接回调，当异步调用连接后，服务器处理连接请求结束后调用，通知调用者连接的状态
    redisAsyncSetConnectCallback(_redis_context,
                                 &redisAsyncOpt::connect_callback);

    // 设置断开连接回调，当服务器断开连接后，通知调用者连接断开，调用者可以利用这个函数实现重连
    redisAsyncSetDisconnectCallback(_redis_context,
                                    &redisAsyncOpt::disconnect_callback);

    // 启动事件线程
    sem_post(&_event_sem);
    return true;
}

bool redisAsyncOpt::disconnect()
{
    if (_redis_context) {
        redisAsyncDisconnect(_redis_context);
        redisAsyncFree(_redis_context);
        _redis_context = NULL;
    }

    return true;
}

bool redisAsyncOpt::publish(const std::string &channel_name,
                              const std::string &message)
{
    int ret = redisAsyncCommand(_redis_context,
                                &redisAsyncOpt::command_callback, this, "PUBLISH %s %s",
                                channel_name.c_str(), message.c_str());
    if (REDIS_ERR == ret) {
        LOG->warn("Publish command failed: {0:d}", ret);
        return false;
    }

    return true;
}

void redisAsyncOpt::connect_callback(const redisAsyncContext *redis_context,
                                       int status)
{
    if (status != REDIS_OK) {
        LOG->warn("Error: {}", redis_context->errstr);
    }
    else {
        LOG->info("Redis connected");
    }
}

void redisAsyncOpt::disconnect_callback(
    const redisAsyncContext *redis_context, int status)
{
    if (status != REDIS_OK) {
        // 这里异常退出，可以尝试重连
    }
}

void redisAsyncOpt::auth_callback(redisAsyncContext *redis_context,
                                  void *r, void *privdata)
{
    redisReply *reply = (redisReply*)r;
    if (reply == nullptr){
        return;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        LOG->warn("AUTH Reply: {}", reply->str);
        return;
    }
    else if (reply->type != REDIS_REPLY_STATUS) {
        LOG->warn("AUTH Reply unknown: {0:d}", reply->type);
        return;
    }
}

void redisAsyncOpt::command_callback(redisAsyncContext *redis_context,
                             void *reply, void *privdata)
{
    // do nothing
}

// 订阅接收回调函数
void redisAsyncOpt::subscriber_callback(redisAsyncContext *redis_context,
                                       void *r, void *privdata)
{
    redisReply *reply = (redisReply*)r;
    if (reply == nullptr){
        return;
    }
    if(reply->type == REDIS_REPLY_ARRAY & reply->elements == 3)
    {
        if(strcmp( reply->element[0]->str,"subscribe") != 0)
        {
            std::string rec = reply->element[2]->str;
            try {
                nlohmann::json js = nlohmann::json::parse(rec);
                std::string host_id;
                std::string msg_id;
                auto obj = js.find("robotId");
                if (obj != js.end()) {
                    host_id = obj.value();
                }
                obj = js.find("msgId");
                if (obj != js.end()) {
                    msg_id = obj.value();
                }

                if (host_id == redisClManager::getHostId()) {
                    std::vector<std::string> command = js["command"];
                    std::vector<std::string> results;
                    results.reserve(command.size());
                    for (const auto& iter : command) {
                        std::string res;
                        redisClManager::execute(iter, res);
                        results.emplace_back(res);
                    }
                    nlohmann::json ret_js;
                    ret_js["robotId"] = host_id;
                    ret_js["msgId"] = msg_id;
                    nlohmann::json array = nlohmann::json::array();
                    for (auto& it : results) {
                        array.push_back(it);
                    }
                    ret_js["errMsg"] = array;
                    std::string ret_str = ret_js.dump();

                    ((redisAsyncOpt *)privdata)->publish("CommandInfoRet", ret_str);
                }
            }
            catch (std::exception& e) {
                LOG->warn("Invalid json string {} : {}", rec, e.what());
            }
        }
    }
}

void * redisAsyncOpt::event_thread(void *data)
{
    if (NULL == data) {
        return NULL;
    }

    redisAsyncOpt*self_this = reinterpret_cast<redisAsyncOpt*>(data);
    return self_this->event_proc();
}

void * redisAsyncOpt::event_proc()
{
    sem_wait(&_event_sem);

    // 开启事件分发，event_base_dispatch会阻塞
    event_base_dispatch(_event_base);

    return NULL;
}
bool redisAsyncOpt::set_subscriber(std::vector<std::string>& topics)
{
    for (const auto& iter : topics) {
        int ret = redisAsyncCommand(_redis_context,
                                    &redisAsyncOpt::subscriber_callback, this, "SUBSCRIBE %s",
                                    iter.c_str());
        if (REDIS_ERR == ret) {
            LOG->warn("SUBSCRIBE command failed: {0:d}", ret);
            return false;
        }
    }
    return true;
}
bool redisAsyncOpt::create()
{
    if (init() && connect()) {
        return true;
    }
    return false;
}

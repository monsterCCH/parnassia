#ifndef PARNASSIA_TRINERVIS_LOGGER_HPP
#define PARNASSIA_TRINERVIS_LOGGER_HPP
#include <syslog.h>
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

#define LOG logger::instance().getLogger()

class logger {
public:
    logger(const logger&) = delete;
    logger &operator=(const logger&) = delete;
    static logger& instance();
    inline spdlog::logger* getLogger() { return p_logger.get(); }
private:
    logger();
    ~logger();

    std::shared_ptr<spdlog::logger> p_logger;
};

logger& logger::instance()
{
    static logger s_instance;
    return s_instance;
}

logger::logger()
{
    try {
        // 从环境变量中设置日志级别，支持指定日志对象 eg: export SPDLOG_LEVEL="off,logger1=debug,logger2=info"
        spdlog::cfg::load_env_levels();
        // 设置日志大小5M，最大3个文件
        p_logger = spdlog::rotating_logger_mt("parnassia", "logs/parnassia.log", 1024 * 1024 * 5, 3);
        p_logger->set_error_handler([](const std::string &msg) { syslog(LOG_ERR, "*** parnassia log err : %s ***", msg.c_str()); });
        p_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [Thread %t] %v");
        spdlog::flush_every(std::chrono::seconds(1));
    } catch (const spdlog::spdlog_ex &e) {
        printf("Log initialization failed: %s\n", e.what());
    }
}

logger::~logger()
{
    spdlog::shutdown();
}

#endif   // PARNASSIA_TRINERVIS_LOGGER_HPP

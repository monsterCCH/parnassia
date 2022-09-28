#include "logger.h"
#include <sys/syslog.h>

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

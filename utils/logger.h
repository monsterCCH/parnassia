#ifndef PARNASSIA_TRINERVIS_LOGGER_H
#define PARNASSIA_TRINERVIS_LOGGER_H

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

#endif   // PARNASSIA_TRINERVIS_LOGGER_H

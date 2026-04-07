#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(const std::string& logFile, const std::string& level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    logger_ = std::make_shared<spdlog::logger>("agent", sinks.begin(), sinks.end());

    spdlog::level::level_enum lvl = spdlog::level::info;
    if (level == "debug") lvl = spdlog::level::debug;
    else if (level == "warning") lvl = spdlog::level::warn;
    else if (level == "error") lvl = spdlog::level::err;
    logger_->set_level(lvl);
    logger_->flush_on(lvl);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return logger_;
}
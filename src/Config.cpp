#include "Config.h"
#include <fstream>
#include <iostream>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open config: " << configFile << std::endl;
        return false;
    }
    json j;
    file >> j;

    uid_ = j.value("uid", "");
    descr_ = j.value("descr", "web-agent");
    serverUrl_ = j.value("server_url", "");
    pollIntervalSec_ = j.value("poll_interval_sec", 10);
    maxRetryIntervalSec_ = j.value("max_retry_interval_sec", 60);
    concurrentTasks_ = j.value("concurrent_tasks", 4);
    taskTimeoutSec_ = j.value("task_timeout_sec", 60);
    logFile_ = j.value("log_file", "./agent.log");
    logLevel_ = j.value("log_level", "info");

    if (uid_.empty() || serverUrl_.empty()) {
        std::cerr << "Missing uid or server_url in config" << std::endl;
        return false;
    }
    return true;
}
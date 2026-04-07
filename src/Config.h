#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Config {
public:
    static Config& getInstance();
    bool load(const std::string& configFile);

    std::string uid() const { return uid_; }
    std::string descr() const { return descr_; }
    std::string serverUrl() const { return serverUrl_; }
    int pollIntervalSec() const { return pollIntervalSec_; }
    int maxRetryIntervalSec() const { return maxRetryIntervalSec_; }
    int concurrentTasks() const { return concurrentTasks_; }
    int taskTimeoutSec() const { return taskTimeoutSec_; }
    std::string logFile() const { return logFile_; }
    std::string logLevel() const { return logLevel_; }

private:
    Config() = default;
    std::string uid_;
    std::string descr_;
    std::string serverUrl_;
    int pollIntervalSec_ = 10;
    int maxRetryIntervalSec_ = 60;
    int concurrentTasks_ = 4;
    int taskTimeoutSec_ = 60;
    std::string logFile_;
    std::string logLevel_ = "info";
};

#endif // CONFIG_H
#include "Agent.h"
#include "Config.h"
#include "Logger.h"
#include "ProcessExecutor.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

Agent::Agent() : running_(false), currentRetryInterval_(5) {}

Agent::~Agent() {
    stop();
}

bool Agent::initialize(const std::string& configFile) {
    if (!Config::getInstance().load(configFile)) {
        return false;
    }
    auto& cfg = Config::getInstance();
    Logger::init(cfg.logFile(), cfg.logLevel());
    Logger::get()->info("Agent initialized with UID: {}", cfg.uid());

    // Create necessary directories
    fs::create_directories("./tasks");
    fs::create_directories("./results");

    return true;
}

void Agent::run() {
    running_ = true;

    // Try to load access code from file
    if (loadAccessCode()) {
        Logger::get()->info("Using existing access code");
    } else {
        // Register with server
        if (!registerWithServer()) {
            Logger::get()->error("Registration failed, exiting");
            return;
        }
        saveAccessCode();
    }

    // Start polling thread
    pollThread_ = std::make_unique<std::thread>(&Agent::pollLoop, this);
    Logger::get()->info("Agent started");
}

void Agent::stop() {
    running_ = false;
    if (pollThread_ && pollThread_->joinable()) {
        pollThread_->join();
    }

    // Wait for all active tasks
    for (auto& f : activeTasks_) {
        if (f.valid()) {
            f.wait();
        }
    }
    Logger::get()->info("Agent stopped");
}

bool Agent::loadAccessCode() {
    std::string filename = Config::getInstance().uid() + ".access";
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    std::getline(file, accessCode_);
    file.close();
    Logger::get()->info("Loaded access code from file: {}", filename);
    return !accessCode_.empty();
}

void Agent::saveAccessCode() {
    std::string filename = Config::getInstance().uid() + ".access";
    std::ofstream file(filename);
    if (file.is_open()) {
        file << accessCode_;
        file.close();
        Logger::get()->info("Saved access code to file: {}", filename);
    } else {
        Logger::get()->error("Failed to save access code to file: {}", filename);
    }
}

bool Agent::registerWithServer() {
    auto& cfg = Config::getInstance();
    std::string url = cfg.serverUrl() + "/wa_reg/";

    json req;
    req["UID"] = cfg.uid();
    req["descr"] = cfg.descr();

    try {
        auto response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{req.dump()}
        );

        if (response.status_code != 200) {
            Logger::get()->error("Registration HTTP error: {}", response.status_code);
            return false;
        }

        auto respJson = json::parse(response.text);
        std::string code = respJson.value("code_responce", "");  // <-- FIXED: field name is code_responce
        if (code == "0") {
            accessCode_ = respJson.value("access_code", "");
            Logger::get()->info("Registration successful, access_code: {}", accessCode_);
            return true;
        } else if (code == "-3") {
            Logger::get()->error("Agent already registered");
            return false;
        } else {
            Logger::get()->error("Registration failed: {}", respJson.value("msg", "unknown"));
            return false;
        }
    } catch (const std::exception& e) {
        Logger::get()->error("Registration exception: {}", e.what());
        return false;
    }
}

void Agent::pollLoop() {
    while (running_) {
        // Check server availability
        std::string healthUrl = Config::getInstance().serverUrl() + "/wa_task/";
        // Simple check: try to get any task (though it will return no task if none)
        // We'll just rely on requestTask to handle errors.

        if (requestTask()) {
            currentRetryInterval_ = Config::getInstance().pollIntervalSec();
        } else {
            // Increase retry interval on failure
            currentRetryInterval_ = std::min(currentRetryInterval_ * 2,
                                             Config::getInstance().maxRetryIntervalSec());
            Logger::get()->warn("Task request failed, next poll in {}s", currentRetryInterval_);
        }

        // Clean up completed tasks from activeTasks_
        activeTasks_.erase(
            std::remove_if(activeTasks_.begin(), activeTasks_.end(),
                [](const std::future<void>& f) {
                    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }),
            activeTasks_.end()
        );

        std::this_thread::sleep_for(std::chrono::seconds(currentRetryInterval_));
    }
}

bool Agent::requestTask() {
    auto& cfg = Config::getInstance();
    std::string url = cfg.serverUrl() + "/wa_task/";

    json req;
    req["UID"] = cfg.uid();
    req["descr"] = cfg.descr();
    req["access_code"] = accessCode_;

    try {
        auto response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{req.dump()},
            cpr::Timeout{10000}
        );

        if (response.status_code != 200) {
            Logger::get()->error("Task request HTTP error: {}", response.status_code);
            return false;
        }

        auto respJson = json::parse(response.text);
        std::string code = respJson.value("code_response", "");
        if (code == "1") {
            // Task available
            Task task;
            task.sessionId = respJson.value("session_id", "");
            task.taskCode = respJson.value("task_code", "");
            task.options = respJson.value("options", "");
            task.accessCode = accessCode_;
            task.uid = cfg.uid();
            task.descr = cfg.descr();

            Logger::get()->info("Received task: code={}, session={}", task.taskCode, task.sessionId);

            // Submit task for execution
            if (activeTasks_.size() < cfg.concurrentTasks()) {
                activeTasks_.push_back(std::async(std::launch::async, &Agent::executeTask, this, task));
            } else {
                std::lock_guard<std::mutex> lock(taskMutex_);
                taskQueue_.push(task);
                Logger::get()->debug("Task queued, queue size: {}", taskQueue_.size());
            }
            return true;
        } else if (code == "0") {
            Logger::get()->debug("No tasks available");
            return true; // no task is not a failure
        } else {
            Logger::get()->error("Task request error: {}", respJson.value("msg", "unknown"));
            return false;
        }
    } catch (const std::exception& e) {
        Logger::get()->error("Task request exception: {}", e.what());
        return false;
    }
}

void Agent::executeTask(const Task& task) {
    Logger::get()->info("Executing task {}", task.taskCode);

    int resultCode = 0;
    std::string message = "Task completed successfully";
    std::string output;
    std::vector<std::string> files;

    // Parse options JSON
    json options;
    try {
        options = json::parse(task.options);
    } catch (...) {
        // If not JSON, treat as raw command
        options["command"] = task.options;
    }

    std::string command = options.value("command", "");
    if (command.empty()) {
        command = task.taskCode; // fallback
    }

    // Execute command
    auto execResult = ProcessExecutor::execute(command, Config::getInstance().taskTimeoutSec());
    output = execResult.output;
    if (execResult.exitCode != 0) {
        resultCode = -1;
        message = "Command failed with exit code " + std::to_string(execResult.exitCode);
        Logger::get()->error("{}: {}", message, output);
    } else {
        Logger::get()->info("Command executed successfully: {}", output);
    }

    // Handle expected files
    if (options.contains("files") && options["files"].is_array()) {
        for (const auto& file : options["files"]) {
            std::string f = file.get<std::string>();
            if (fs::exists(f)) {
                files.push_back(f);
                Logger::get()->debug("Found expected file: {}", f);
            } else {
                Logger::get()->warn("Expected file not found: {}", f);
            }
        }
    }

    // Upload results
    if (!uploadResult(task, resultCode, message, files, output)) {
        Logger::get()->error("Failed to upload results for task {}", task.taskCode);
    }
}

bool Agent::uploadResult(const Task& task, int resultCode, const std::string& message,
                         const std::vector<std::string>& files, const std::string& output) {
    auto& cfg = Config::getInstance();
    std::string url = cfg.serverUrl() + "/wa_result/";

    // Prepare the result JSON
    json resultJson;
    resultJson["UID"] = task.uid;
    resultJson["access_code"] = task.accessCode;
    resultJson["message"] = message;
    resultJson["files"] = files.size();
    resultJson["session_id"] = task.sessionId;
    resultJson["output"] = output;

    // Build parts vector
    std::vector<cpr::Part> parts;
    parts.emplace_back("result_code", std::to_string(resultCode));
    parts.emplace_back("result", resultJson.dump());

    // Attach files
    for (size_t i = 0; i < files.size(); ++i) {
        parts.emplace_back("file" + std::to_string(i + 1), cpr::File{files[i]});
    }

    // Construct Multipart with parts
    cpr::Multipart multipart(parts);

    try {
        auto response = cpr::Post(cpr::Url{url}, multipart, cpr::Timeout{30000});
        if (response.status_code != 200) {
            Logger::get()->error("Upload HTTP error: {}", response.status_code);
            return false;
        }
        auto respJson = json::parse(response.text);
        std::string code = respJson.value("code_responce", "");
        if (code == "0") {
            Logger::get()->info("Upload successful for session {}", task.sessionId);
            return true;
        } else {
            Logger::get()->error("Upload failed: {}", respJson.value("msg", "unknown"));
            return false;
        }
    } catch (const std::exception& e) {
        Logger::get()->error("Upload exception: {}", e.what());
        return false;
    }
}
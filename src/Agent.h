#ifndef AGENT_H
#define AGENT_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <future>
#include <queue>
#include <mutex>
#include "Task.h"

class Agent {
public:
    Agent();
    ~Agent();

    bool initialize(const std::string& configFile);
    void run();
    void stop();

private:
    bool loadAccessCode();
    void saveAccessCode();
    void pollLoop();
    bool registerWithServer();
    bool requestTask();
    void executeTask(const Task& task);
    bool uploadResult(const Task& task, int resultCode, const std::string& message,
                      const std::vector<std::string>& files, const std::string& output);

    std::atomic<bool> running_;
    std::unique_ptr<std::thread> pollThread_;
    int currentRetryInterval_;
    std::string accessCode_;  // received during registration
    std::vector<std::future<void>> activeTasks_;
    std::queue<Task> taskQueue_;
    std::mutex taskMutex_;
};

#endif // AGENT_H
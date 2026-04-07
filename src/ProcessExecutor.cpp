#include "ProcessExecutor.h"
#include "Logger.h"
#include <cstdio>
#include <array>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

ProcessResult ProcessExecutor::execute(const std::string& command, int timeoutSec) {
    ProcessResult result;
    result.exitCode = -1;

    // Run command in a thread with timeout
    auto future = std::async(std::launch::async, [command, &result]() {
#ifdef _WIN32
        std::string cmd = command + " 2>&1";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) return -1;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result.output += buffer;
        }
        int status = _pclose(pipe);
        result.exitCode = status;
        return status;
#else
        std::string cmd = command + " 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return -1;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result.output += buffer;
        }
        int status = pclose(pipe);
        if (WIFEXITED(status)) {
            result.exitCode = WEXITSTATUS(status);
        } else {
            result.exitCode = -1;
        }
        return result.exitCode;
#endif
    });

    if (future.wait_for(std::chrono::seconds(timeoutSec)) == std::future_status::timeout) {
        Logger::get()->error("Process timed out after {} seconds", timeoutSec);
        result.error = "Timeout";
        result.exitCode = -1;
        // TODO: attempt to kill the process (platform-specific)
        // For simplicity, we leave it running (detached). In production, need to kill.
    }
    return result;
}
#ifndef PROCESS_EXECUTOR_H
#define PROCESS_EXECUTOR_H

#include <string>
#include <vector>
#include <future>

struct ProcessResult {
    int exitCode;
    std::string output;
    std::string error;
};

class ProcessExecutor {
public:
    static ProcessResult execute(const std::string& command, int timeoutSec = 60);
};

#endif // PROCESS_EXECUTOR_H
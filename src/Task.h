#ifndef TASK_H
#define TASK_H

#include <string>
#include <vector>

struct Task {
    std::string sessionId;
    std::string taskCode;   // e.g., "CONF"
    std::string options;    // JSON string with command, files, etc.
    std::string accessCode; // from registration
    std::string uid;
    std::string descr;
};

#endif // TASK_H
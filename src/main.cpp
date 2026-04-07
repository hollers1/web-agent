#include "Agent.h"
#include <iostream>
#include <signal.h>
#include <memory>

std::unique_ptr<Agent> g_agent;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    if (g_agent) {
        g_agent->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    std::string configFile = "config/agent_config.json";
    if (argc > 1) {
        configFile = argv[1];
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    g_agent = std::make_unique<Agent>();
    if (!g_agent->initialize(configFile)) {
        std::cerr << "Failed to initialize agent" << std::endl;
        return 1;
    }

    g_agent->run();

    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
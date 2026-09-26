#include "sandboxx/monitor.hpp"
#include <iostream>
#include <fstream>
#include <string>

namespace sandboxx {
void Monitor::inspect(pid_t pid) {
    std::cout << "[SandBoxX] Inspecting PID " << pid << "\n";
    std::ifstream stat("/proc/" + std::to_string(pid) + "/status");
    if (!stat) {
        std::cout << "Process not found.\n";
        return;
    }
    std::string line;
    while (std::getline(stat, line)) {
        if (line.rfind("Name:", 0) == 0 ||
            line.rfind("State:", 0) == 0 ||
            line.rfind("VmRSS:", 0) == 0) {
            std::cout << line << "\n";
        }
    }
}
}

#pragma once
#include "sandboxx/sandbox_config.hpp"
#include <string>
#include <vector>
namespace sandboxx {
// Exit codes reserved by the runtime itself (the sandboxed command's own
// exit code is passed through otherwise).
constexpr int kRuntimeError = 125;   // SandBoxX failed before the command ran
constexpr int kSetupError = 126;     // sandbox setup failed or command not executable
constexpr int kNotFound = 127;       // command not found

class Runtime {
public:
    explicit Runtime(SandboxConfig config = {});
    int run(const std::vector<std::string>& args);
private:
    SandboxConfig config_;
};
}

#pragma once
#include "sandboxx/runtime.hpp"
#include "sandboxx/sandbox_config.hpp"
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace test {
constexpr int kSkip = 77;  // CTest SKIP_RETURN_CODE
inline int failures = 0;

inline void check(bool ok, const std::string& name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
    if (!ok) ++failures;
}

inline void check_eq(long long actual, long long expected, const std::string& name) {
    check(actual == expected, name);
    if (actual != expected)
        std::cout << "     expected " << expected << ", got " << actual << std::endl;
}

inline int finish() {
    std::cout << (failures ? "FAILED" : "ALL PASSED") << std::endl;
    return failures ? 1 : 0;
}

// True (after saying why) when privileged Linux tests cannot run here.
inline bool skip_unless_linux_root() {
#ifdef __linux__
    if (geteuid() == 0) return false;
    std::cout << "SKIP: requires root" << std::endl;
#else
    std::cout << "SKIP: requires Linux" << std::endl;
#endif
    return true;
}

// A config with every isolation feature and limit turned off.
inline sandboxx::SandboxConfig bare_config() {
    sandboxx::SandboxConfig config;
    config.cpu_percent = 0;
    config.memory_mb = 0;
    config.pid_namespace = false;
    config.mount_namespace = false;
    config.network_namespace = false;
    config.restricted_filesystem = false;
    config.security_policy = false;
    return config;
}

inline int run(const sandboxx::SandboxConfig& config, const std::vector<std::string>& args) {
    return sandboxx::Runtime(config).run(args);
}
}

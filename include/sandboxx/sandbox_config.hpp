#pragma once
#include <string>

namespace sandboxx {
struct SandboxConfig {
    int cpu_percent = 50;          // percent of one CPU; 0 = unlimited
    long memory_mb = 128;          // 0 = unlimited
    bool pid_namespace = true;
    bool mount_namespace = true;
    bool network_namespace = true;
    bool restricted_filesystem = true;
    bool security_policy = true;
    int timeout_seconds = 0;       // 0 = no timeout
    std::string hostname = "sandboxx";
    std::string rootfs = "./rootfs";
};

// Parses a flat JSON object of config keys, overriding the values already in
// config. Unknown keys and wrongly typed values are errors.
bool parse_config(const std::string& json, SandboxConfig& config, std::string& error);
bool load_config(const std::string& path, SandboxConfig& config, std::string& error);
bool validate_config(const SandboxConfig& config, std::string& error);
}

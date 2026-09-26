#include "test_util.hpp"

namespace {
bool parses(const std::string& json, sandboxx::SandboxConfig& config) {
    std::string error;
    bool ok = sandboxx::parse_config(json, config, error);
    if (!ok) std::cout << "     error: " << error << std::endl;
    return ok;
}

bool rejects(const std::string& json) {
    sandboxx::SandboxConfig config;
    std::string error;
    return !sandboxx::parse_config(json, config, error) && !error.empty();
}

bool valid(const sandboxx::SandboxConfig& config) {
    std::string error;
    return sandboxx::validate_config(config, error);
}
}

int main(int argc, char* argv[]) {
    using test::check;
    using test::check_eq;

    sandboxx::SandboxConfig config;
    check(parses(R"({"cpu_percent": 25, "memory_mb": 64, "network_namespace": false,
                    "hostname": "box", "rootfs": "/srv/root", "timeout_seconds": 5})",
                 config),
          "parses all value types");
    check_eq(config.cpu_percent, 25, "cpu_percent read");
    check_eq(config.memory_mb, 64, "memory_mb read");
    check_eq(config.timeout_seconds, 5, "timeout_seconds read");
    check(!config.network_namespace, "boolean read");
    check(config.pid_namespace, "unspecified keys keep defaults");
    check(config.hostname == "box" && config.rootfs == "/srv/root", "strings read");

    sandboxx::SandboxConfig empty;
    check(parses("  { }  ", empty), "empty object is accepted");

    check(rejects(R"({"cpu": 10})"), "unknown key rejected");
    check(rejects(R"({"cpu_percent": "10"})"), "wrong type rejected");
    check(rejects(R"({"cpu_percent": 1.5})"), "non-integer rejected");
    check(rejects(R"({"cpu_percent": 99999999999})"), "out-of-range integer rejected");
    check(rejects(R"({"cpu_percent": 10,})"), "trailing comma rejected");
    check(rejects(R"({"cpu_percent": 10} extra)"), "trailing content rejected");
    check(rejects(R"({"cpu_percent": 10)"), "unterminated object rejected");

    sandboxx::SandboxConfig before;
    sandboxx::SandboxConfig after = before;
    std::string error;
    sandboxx::parse_config(R"({"cpu_percent": 10, "bogus": 1})", after, error);
    check_eq(after.cpu_percent, before.cpu_percent, "failed parse leaves config unchanged");

    check(valid(sandboxx::SandboxConfig{}), "defaults are valid");
    sandboxx::SandboxConfig bad;
    bad.mount_namespace = false;
    check(!valid(bad), "restricted filesystem requires mount namespace");
    bad = {};
    bad.cpu_percent = -1;
    check(!valid(bad), "negative cpu rejected");
    bad = {};
    bad.hostname = "";
    check(!valid(bad), "empty hostname rejected");

    // Shipped config files must load and validate.
    for (int i = 1; i < argc; ++i) {
        sandboxx::SandboxConfig file_config;
        bool ok = sandboxx::load_config(argv[i], file_config, error) && valid(file_config);
        if (!ok) std::cout << "     error: " << error << std::endl;
        check(ok, std::string("loads ") + argv[i]);
    }
    return test::finish();
}

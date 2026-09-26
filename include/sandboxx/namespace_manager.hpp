#pragma once
#include "sandboxx/sandbox_config.hpp"
namespace sandboxx {
class NamespaceManager {
public:
    // clone(2) flags for the namespaces enabled in config; 0 if none are.
    // UTS and IPC namespaces are added whenever any namespace is enabled.
    static int clone_flags(const SandboxConfig& config);
    // Runs inside the new namespaces: sets the hostname, makes mounts private
    // and brings up loopback.
    bool setup(const SandboxConfig& config);
    // Mounts a /proc that shows only the new PID namespace. Runs after the
    // filesystem has been restricted.
    bool mount_proc();
};
}

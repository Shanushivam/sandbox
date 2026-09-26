#include "sandboxx/runtime.hpp"
#include "sandboxx/process_manager.hpp"
#include "sandboxx/namespace_manager.hpp"
#include "sandboxx/cgroup_manager.hpp"
#include "sandboxx/filesystem_manager.hpp"
#include "sandboxx/security_manager.hpp"
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <utility>

namespace sandboxx {
namespace {
void print_summary(const SandboxConfig& c) {
    auto on = [](bool b) { return b ? "on" : "off"; };
    std::cout << "[SandBoxX] Starting sandbox: pid-ns " << on(c.pid_namespace)
              << ", mount-ns " << on(c.mount_namespace)
              << ", net-ns " << on(c.network_namespace)
              << ", rootfs " << (c.restricted_filesystem ? c.rootfs : "off")
              << ", seccomp " << on(c.security_policy)
              << ", cpu " << (c.cpu_percent ? std::to_string(c.cpu_percent) + "%" : "unlimited")
              << ", memory " << (c.memory_mb ? std::to_string(c.memory_mb) + " MB" : "unlimited")
              << ", timeout " << (c.timeout_seconds ? std::to_string(c.timeout_seconds) + "s" : "none")
              << "\n";
}
}

Runtime::Runtime(SandboxConfig config) : config_(std::move(config)) {}

int Runtime::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "[SandBoxX] No command given.\n";
        return kRuntimeError;
    }
    std::string error;
    if (!validate_config(config_, error)) {
        std::cerr << "[SandBoxX] Invalid config: " << error << "\n";
        return kRuntimeError;
    }

    const int clone_flags = NamespaceManager::clone_flags(config_);
    const bool limited = config_.cpu_percent > 0 || config_.memory_mb > 0;
    const bool isolated = config_.pid_namespace || config_.mount_namespace ||
                          config_.network_namespace || config_.restricted_filesystem ||
                          config_.security_policy;
#ifdef __linux__
    if ((isolated || limited) && geteuid() != 0) {
        std::cerr << "[SandBoxX] Isolation and resource limits require root (try sudo).\n";
        return kRuntimeError;
    }
#else
    if (isolated || limited) {
        std::cerr << "[SandBoxX] Isolation and resource limits are only supported on Linux.\n";
        return kRuntimeError;
    }
#endif

    print_summary(config_);

    CgroupManager cg;
    if (limited && !cg.create("sandboxx-" + std::to_string(getpid()),
                              config_.cpu_percent, config_.memory_mb)) {
        return kRuntimeError;
    }

    // Everything below runs inside the child, between fork and exec, so that
    // it confines the command rather than SandBoxX itself. Order matters: the
    // cgroup is joined while /sys/fs/cgroup is still visible, and the security
    // policy goes last because it forbids the mount calls made before it.
    NamespaceManager ns;
    FilesystemManager fs;
    SecurityManager sec;
    auto child_setup = [&]() {
        if (limited && !cg.attach_self()) return false;
        if (clone_flags != 0 && !ns.setup(config_)) return false;
        if (config_.restricted_filesystem && !fs.setup(config_.rootfs)) return false;
        if (config_.pid_namespace && config_.mount_namespace && !ns.mount_proc()) return false;
        if (config_.security_policy && !sec.apply()) return false;
        return true;
    };

    ProcessManager pm;
    pid_t pid = pm.start(args, child_setup, clone_flags);
    if (pid < 0) {
        std::perror("[SandBoxX] Failed to start process");
        cg.destroy();
        return kRuntimeError;
    }

    std::cout << "[SandBoxX] Process started with PID: " << pid << std::endl;

    int code = pm.wait(pid, config_.timeout_seconds);
    cg.destroy();
    if (code < 0) {
        std::cerr << "[SandBoxX] Failed to wait for process " << pid << ".\n";
        return kRuntimeError;
    }

    std::cout << "[SandBoxX] Process " << pid << " exited with code " << code << "\n";
    return code;
}
}

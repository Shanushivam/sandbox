#include "sandboxx/namespace_manager.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#ifdef __linux__
#include <net/if.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sandboxx {
namespace {
#ifdef __linux__
bool fail(const char* what) {
    std::cerr << "[SandBoxX] " << what << ": " << std::strerror(errno) << "\n";
    return false;
}

bool bring_up_loopback() {
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return fail("socket");
    ifreq ifr {};
    std::strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
    bool ok = ioctl(fd, SIOCGIFFLAGS, &ifr) == 0;
    if (ok) {
        ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
        ok = ioctl(fd, SIOCSIFFLAGS, &ifr) == 0;
    }
    if (!ok) fail("bring up loopback");
    close(fd);
    return ok;
}
#endif
}

int NamespaceManager::clone_flags(const SandboxConfig& config) {
#ifdef __linux__
    if (!config.pid_namespace && !config.mount_namespace && !config.network_namespace) return 0;
    int flags = CLONE_NEWUTS | CLONE_NEWIPC;
    if (config.pid_namespace) flags |= CLONE_NEWPID;
    if (config.mount_namespace) flags |= CLONE_NEWNS;
    if (config.network_namespace) flags |= CLONE_NEWNET;
    return flags;
#else
    (void)config;
    return 0;
#endif
}

bool NamespaceManager::setup(const SandboxConfig& config) {
#ifdef __linux__
    if (sethostname(config.hostname.c_str(), config.hostname.size()) != 0)
        return fail("sethostname");
    // Stop mount changes inside the sandbox from propagating to the host.
    if (config.mount_namespace &&
        mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0)
        return fail("make mounts private");
    if (config.network_namespace && !bring_up_loopback()) return false;
    return true;
#else
    (void)config;
    std::cerr << "[SandBoxX] Namespaces are only supported on Linux.\n";
    return false;
#endif
}

bool NamespaceManager::mount_proc() {
#ifdef __linux__
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) != 0)
        return fail("mount /proc");
    return true;
#else
    std::cerr << "[SandBoxX] Namespaces are only supported on Linux.\n";
    return false;
#endif
}
}

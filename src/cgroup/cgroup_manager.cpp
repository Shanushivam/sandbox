#include "sandboxx/cgroup_manager.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace sandboxx {
namespace {
const std::string kCgroupRoot = "/sys/fs/cgroup";

bool write_file(const std::string& path, const std::string& value) {
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t written = write(fd, value.data(), value.size());
    int err = errno;
    close(fd);
    errno = err;
    return written == static_cast<ssize_t>(value.size());
}

bool fail(const std::string& what) {
    std::cerr << "[SandBoxX] " << what << ": " << std::strerror(errno) << "\n";
    return false;
}

#ifdef __linux__
bool controller_enabled(const std::string& controller) {
    std::ifstream file(kCgroupRoot + "/cgroup.subtree_control");
    std::string name;
    while (file >> name) {
        if (name == controller) return true;
    }
    return false;
}

bool enable_controller(const std::string& controller) {
    if (controller_enabled(controller)) return true;
    if (write_file(kCgroupRoot + "/cgroup.subtree_control", "+" + controller)) return true;
    return fail("enable cgroup controller '" + controller + "'");
}
#endif
}

bool CgroupManager::create(const std::string& name, int cpu_percent, long memory_mb) {
#ifdef __linux__
    if (access((kCgroupRoot + "/cgroup.controllers").c_str(), F_OK) != 0) {
        std::cerr << "[SandBoxX] cgroup v2 is not mounted at " << kCgroupRoot << ".\n";
        return false;
    }
    if (cpu_percent > 0 && !enable_controller("cpu")) return false;
    if (memory_mb > 0 && !enable_controller("memory")) return false;

    std::string path = kCgroupRoot + "/" + name;
    if (mkdir(path.c_str(), 0755) != 0) return fail("create cgroup " + path);
    path_ = path;

    if (cpu_percent > 0) {
        // cpu.max is "<quota> <period>" in microseconds; 100% = one full CPU.
        constexpr long kPeriod = 100000;
        long quota = static_cast<long>(cpu_percent) * kPeriod / 100;
        if (!write_file(path_ + "/cpu.max", std::to_string(quota) + " " + std::to_string(kPeriod))) {
            fail("set cpu.max");
            destroy();
            return false;
        }
    }
    if (memory_mb > 0) {
        long long bytes = static_cast<long long>(memory_mb) * 1024 * 1024;
        if (!write_file(path_ + "/memory.max", std::to_string(bytes))) {
            fail("set memory.max");
            destroy();
            return false;
        }
        // Without this the limit could be dodged by swapping; absent when
        // swap accounting is disabled, which is fine.
        write_file(path_ + "/memory.swap.max", "0");
    }
    return true;
#else
    (void)name;
    (void)cpu_percent;
    (void)memory_mb;
    std::cerr << "[SandBoxX] cgroups are only supported on Linux.\n";
    return false;
#endif
}

bool CgroupManager::attach_self() const {
    // Writing 0 moves the writing process.
    if (!write_file(path_ + "/cgroup.procs", "0")) return fail("join cgroup " + path_);
    return true;
}

bool CgroupManager::destroy() {
    if (path_.empty()) return true;
    if (rmdir(path_.c_str()) != 0 && errno == EBUSY) {
        // Processes are still inside (e.g. background jobs of the command).
        write_file(path_ + "/cgroup.kill", "1");
        for (int attempt = 0; attempt < 50; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (rmdir(path_.c_str()) == 0 || errno != EBUSY) break;
        }
    }
    if (access(path_.c_str(), F_OK) == 0) return fail("remove cgroup " + path_);
    path_.clear();
    return true;
}
}

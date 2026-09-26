#pragma once
#include <string>
namespace sandboxx {
// Manages a cgroup v2 group under /sys/fs/cgroup.
class CgroupManager {
public:
    // Creates the group with the given limits (0 = unlimited).
    bool create(const std::string& name, int cpu_percent, long memory_mb);
    // Moves the calling process into the group; called from the sandboxed child.
    bool attach_self() const;
    // Kills anything left in the group and removes it.
    bool destroy();
    const std::string& path() const { return path_; }
private:
    std::string path_;
};
}

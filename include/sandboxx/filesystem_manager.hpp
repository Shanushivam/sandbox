#pragma once
#include <string>
namespace sandboxx {
class FilesystemManager {
public:
    // Makes rootfs the root directory (read-only, with a writable /tmp and
    // basic /dev nodes) and detaches the host filesystem. Must run inside a
    // private mount namespace.
    bool setup(const std::string& rootfs);
};
}

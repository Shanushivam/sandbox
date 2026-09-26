#include "sandboxx/filesystem_manager.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/mount.h>
#include <sys/syscall.h>
#endif

namespace sandboxx {
namespace {
bool fail(const std::string& what) {
    std::cerr << "[SandBoxX] " << what << ": " << std::strerror(errno) << "\n";
    return false;
}

bool ensure_dir(const std::string& path, mode_t mode) {
    if (mkdir(path.c_str(), mode) == 0 || errno == EEXIST) return true;
    return fail("mkdir " + path);
}

#ifdef __linux__
bool bind_device(const std::string& root, const char* name) {
    std::string target = root + "/dev/" + name;
    int fd = open(target.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666);
    if (fd < 0) return fail("create " + target);
    close(fd);
    std::string source = std::string("/dev/") + name;
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) != 0)
        return fail("bind " + source);
    return true;
}
#endif
}

bool FilesystemManager::setup(const std::string& rootfs) {
#ifdef __linux__
    char resolved[PATH_MAX];
    if (!realpath(rootfs.c_str(), resolved)) return fail("rootfs " + rootfs);
    const std::string root = resolved;

    // pivot_root needs the new root to be a mount point.
    if (mount(root.c_str(), root.c_str(), nullptr, MS_BIND | MS_REC, nullptr) != 0)
        return fail("bind mount rootfs");

    for (const char* dir : {"/dev", "/proc", "/tmp", "/.oldroot"}) {
        if (!ensure_dir(root + dir, 0755)) return false;
    }
    for (const char* dev : {"null", "zero", "full", "random", "urandom"}) {
        if (!bind_device(root, dev)) return false;
    }
    if (mount("tmpfs", (root + "/tmp").c_str(), "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") != 0)
        return fail("mount /tmp");

    if (syscall(SYS_pivot_root, root.c_str(), (root + "/.oldroot").c_str()) != 0)
        return fail("pivot_root");
    if (chdir("/") != 0) return fail("chdir /");
    if (umount2("/.oldroot", MNT_DETACH) != 0) return fail("detach host filesystem");
    rmdir("/.oldroot");

    // Everything except /tmp and the device nodes is read-only.
    if (mount(nullptr, "/", nullptr, MS_REMOUNT | MS_BIND | MS_RDONLY | MS_NOSUID, nullptr) != 0)
        return fail("remount rootfs read-only");
    return true;
#else
    (void)rootfs;
    (void)ensure_dir;
    std::cerr << "[SandBoxX] Filesystem restriction is only supported on Linux.\n";
    return false;
#endif
}
}

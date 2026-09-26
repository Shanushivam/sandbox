#include "test_util.hpp"

int main(int argc, char* argv[]) {
    if (test::skip_unless_linux_root()) return test::kSkip;
    std::string rootfs = argc > 1 ? argv[1] : "rootfs";
    if (access((rootfs + "/bin/sh").c_str(), X_OK) != 0) {
        std::cout << "SKIP: " << rootfs << " is not populated (run scripts/setup_rootfs.sh)"
                  << std::endl;
        return test::kSkip;
    }
    using test::check_eq;

    sandboxx::SandboxConfig config = test::bare_config();
    config.pid_namespace = true;
    config.mount_namespace = true;
    config.restricted_filesystem = true;
    config.rootfs = rootfs;

    check_eq(test::run(config, {"/bin/sh", "-c", "test ! -e /etc/passwd"}), 0,
             "host files are not visible");
    check_eq(test::run(config, {"/bin/sh", "-c", "test ! -e /.oldroot"}), 0,
             "old root is detached");
    check_eq(test::run(config, {"/bin/sh", "-c", "! echo x 2>/dev/null > /escape"}), 0,
             "root filesystem is read-only");
    check_eq(test::run(config, {"/bin/sh", "-c", "echo x > /tmp/file && test -s /tmp/file"}), 0,
             "/tmp is writable");
    check_eq(test::run(config, {"/bin/sh", "-c", "echo x > /dev/null"}), 0,
             "/dev/null works");
    check_eq(test::run(config, {"/bin/sh", "-c", "test -d /proc/1"}), 0,
             "/proc is mounted");
    return test::finish();
}

#include "test_util.hpp"

int main() {
    if (test::skip_unless_linux_root()) return test::kSkip;
    using test::check_eq;

    sandboxx::SandboxConfig config = test::bare_config();
    config.pid_namespace = true;
    config.mount_namespace = true;
    config.network_namespace = true;
    config.hostname = "sandboxx-test";

    check_eq(test::run(config, {"sh", "-c", "test $$ -eq 2"}), 0,
             "command runs as PID 2 under the sandbox init");
    check_eq(test::run(config, {"sh", "-c", "test $(ls /proc | grep -c '^[0-9]*$') -lt 10"}), 0,
             "host processes are not visible");
    check_eq(test::run(config, {"sh", "-c",
                                "test \"$(cat /proc/sys/kernel/hostname)\" = sandboxx-test"}),
             0, "hostname is isolated");
    check_eq(test::run(config, {"sh", "-c", "test $(grep -c : /proc/net/dev) -eq 1"}), 0,
             "only loopback in network namespace");

    char host[256] {};
    gethostname(host, sizeof(host) - 1);
    test::check(std::string(host) != "sandboxx-test", "host hostname unchanged");

    check_eq(test::run(config, {"sh", "-c", "exit 5"}), 5, "exit code passes through init");
    check_eq(test::run(config, {"sh", "-c", "kill -9 $$"}), 128 + 9,
             "signal death passes through init");
    config.timeout_seconds = 1;
    check_eq(test::run(config, {"sleep", "10"}), 128 + 15,
             "init forwards SIGTERM on timeout");
    return test::finish();
}

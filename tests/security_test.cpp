#include "test_util.hpp"

int main() {
    if (test::skip_unless_linux_root()) return test::kSkip;
    using test::check_eq;

    sandboxx::SandboxConfig config = test::bare_config();
    config.security_policy = true;

    check_eq(test::run(config, {"sh", "-c", "grep -q '^CapEff:[[:space:]]*0*$' /proc/self/status"}),
             0, "all capabilities dropped");
    check_eq(test::run(config, {"sh", "-c", "grep -q '^CapBnd:[[:space:]]*0*$' /proc/self/status"}),
             0, "capability bounding set emptied");
    check_eq(test::run(config, {"sh", "-c", "grep -q '^NoNewPrivs:[[:space:]]*1' /proc/self/status"}),
             0, "no_new_privs set");
    check_eq(test::run(config, {"sh", "-c", "grep -q '^Seccomp:[[:space:]]*2' /proc/self/status"}),
             0, "seccomp filter installed");
    check_eq(test::run(config, {"sh", "-c", "echo 1"}), 0, "normal commands still work");
    return test::finish();
}

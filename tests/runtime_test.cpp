#include "sandboxx/process_manager.hpp"
#include "test_util.hpp"

namespace {
int run_and_wait(const std::vector<std::string>& args,
                 const std::function<bool()>& setup = {}) {
    sandboxx::ProcessManager pm;
    pid_t pid = pm.start(args, setup);
    if (pid < 0) return -2;
    return pm.wait(pid);
}
}

int main() {
    using test::check_eq;
    check_eq(run_and_wait({"true"}), 0, "success exit code");
    check_eq(run_and_wait({"false"}), 1, "failure exit code is propagated");
    check_eq(run_and_wait({"sh", "-c", "exit 7"}), 7, "arbitrary exit code");
    check_eq(run_and_wait({"sh", "-c", "test \"$1\" = 'a b'", "sh", "a b"}), 0,
             "arguments with spaces are passed intact");
    check_eq(run_and_wait({"sh", "-c", "exit 3", "sh", "; exit 9"}), 3,
             "arguments are not interpreted by a shell");
    check_eq(run_and_wait({"/nonexistent/sandboxx-command"}),
             sandboxx::kNotFound, "missing command exits 127");
    check_eq(run_and_wait({"true"}, [] { return false; }),
             sandboxx::kSetupError, "failed child setup exits 126");
    check_eq(run_and_wait({"sh", "-c", "kill -9 $$"}), 128 + 9,
             "signal death reported as 128 + signal");

    sandboxx::ProcessManager pm;
    pid_t pid = pm.start({"sleep", "10"});
    check_eq(pm.stop(pid) ? 1 : 0, 1, "stop sends SIGTERM");
    check_eq(pm.wait(pid), 128 + 15, "stopped process reports SIGTERM");

    pid = pm.start({"sleep", "10"});
    check_eq(pm.wait(pid, 1), 128 + 15, "timeout terminates the process");
    pid = pm.start({"sh", "-c", "trap '' TERM; sleep 10"});
    check_eq(pm.wait(pid, 1), 128 + 9, "process ignoring SIGTERM is killed");

    check_eq(test::run(test::bare_config(), {}), sandboxx::kRuntimeError,
             "runtime rejects empty command");
    check_eq(test::run(test::bare_config(), {"sh", "-c", "exit 3"}), 3,
             "runtime propagates command exit code");
    sandboxx::SandboxConfig invalid = test::bare_config();
    invalid.memory_mb = -1;
    check_eq(test::run(invalid, {"true"}), sandboxx::kRuntimeError,
             "runtime rejects invalid config");
#ifdef __linux__
    if (geteuid() != 0)
        check_eq(test::run({}, {"true"}), sandboxx::kRuntimeError,
                 "isolation without root is refused");
#else
    check_eq(test::run({}, {"true"}), sandboxx::kRuntimeError,
             "isolation off Linux is refused instead of running unsandboxed");
#endif
    return test::finish();
}

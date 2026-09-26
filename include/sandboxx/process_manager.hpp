#pragma once
#include <sys/types.h>
#include <functional>
#include <string>
#include <vector>
namespace sandboxx {
class ProcessManager {
public:
    // Starts args[0] (PATH lookup, no shell). child_setup runs in the command's
    // process before exec; if it returns false the command exits with 126.
    //
    // With non-zero clone_flags (Linux only) the child is created in new
    // namespaces and runs a minimal init as PID 1, which starts the command,
    // forwards termination signals to it and reaps orphaned processes.
    pid_t start(const std::vector<std::string>& args,
                const std::function<bool()>& child_setup = {},
                int clone_flags = 0);
    // Blocks until pid exits. Returns its exit code, 128 + signal if it was
    // killed by a signal, or -1 on error. With a timeout, the process gets
    // SIGTERM when it expires and SIGKILL after a further grace period.
    int wait(pid_t pid, int timeout_seconds = 0);
    bool stop(pid_t pid);
};
}

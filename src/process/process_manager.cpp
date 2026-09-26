#include "sandboxx/process_manager.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
#endif

namespace sandboxx {
namespace {
constexpr int kGraceSeconds = 2;

struct ChildContext {
    char** argv;
    const std::function<bool()>* setup;
};

void flush_all() {
    std::cout.flush();
    std::cerr.flush();
    std::fflush(nullptr);
}

[[noreturn]] void exec_command(const ChildContext& ctx) {
    if (*ctx.setup && !(*ctx.setup)()) {
        flush_all();
        _exit(126);
    }
    // exec discards unflushed buffers, so flush anything setup printed.
    flush_all();
    execvp(ctx.argv[0], ctx.argv);
    int err = errno;
    std::perror((std::string("[SandBoxX] exec ") + ctx.argv[0]).c_str());
    _exit(err == ENOENT ? 127 : 126);
}

int decode_status(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

#ifdef __linux__
constexpr int kForwardedSignals[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGUSR1, SIGUSR2};
volatile sig_atomic_t g_command_pid = 0;

void forward_signal(int sig) {
    if (g_command_pid > 0) kill(g_command_pid, sig);
}

// Entry point of the cloned child, PID 1 of the new PID namespace. The kernel
// only delivers signals to PID 1 that it has handlers for, so the command
// runs as a separate process and this init relays signals to it.
int init_main(void* arg) {
    auto* ctx = static_cast<ChildContext*>(arg);

    struct sigaction sa {};
    sa.sa_handler = forward_signal;
    sigemptyset(&sa.sa_mask);
    for (int sig : kForwardedSignals) sigaction(sig, &sa, nullptr);

    pid_t command = fork();
    if (command < 0) {
        std::perror("[SandBoxX] fork");
        _exit(125);
    }
    if (command == 0) {
        for (int sig : kForwardedSignals) signal(sig, SIG_DFL);
        exec_command(*ctx);
    }
    g_command_pid = command;

    while (true) {
        int status = 0;
        pid_t done = waitpid(-1, &status, 0);
        if (done < 0) {
            if (errno == EINTR) continue;
            _exit(125);
        }
        // Other children are orphans that were re-parented to us; just reap them.
        if (done == command) {
            int code = decode_status(status);
            _exit(code < 0 ? 125 : code);
        }
    }
}
#endif
}

pid_t ProcessManager::start(const std::vector<std::string>& args,
                            const std::function<bool()>& child_setup,
                            int clone_flags) {
    if (args.empty()) return -1;

    // Build argv before forking so the child does no allocation it could avoid.
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    ChildContext ctx{argv.data(), &child_setup};

    // Flush buffered output so it is neither duplicated in nor reordered by the child.
    flush_all();

    if (clone_flags != 0) {
#ifdef __linux__
        constexpr size_t kStackSize = 1024 * 1024;
        void* stack = mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
        if (stack == MAP_FAILED) return -1;
        // Without CLONE_VM the child gets its own copy of this memory, so the
        // parent can release its mapping straight away.
        pid_t pid = clone(init_main, static_cast<char*>(stack) + kStackSize,
                          clone_flags | SIGCHLD, &ctx);
        int err = errno;
        munmap(stack, kStackSize);
        errno = err;
        return pid;
#else
        errno = ENOSYS;
        return -1;
#endif
    }

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) exec_command(ctx);
    return pid;
}

int ProcessManager::wait(pid_t pid, int timeout_seconds) {
    using Clock = std::chrono::steady_clock;
    int status = 0;

    if (timeout_seconds <= 0) {
        pid_t result;
        do {
            result = waitpid(pid, &status, 0);
        } while (result < 0 && errno == EINTR);
        return result < 0 ? -1 : decode_status(status);
    }

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);
    bool terminated = false;
    while (true) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return decode_status(status);
        if (result < 0 && errno != EINTR) return -1;

        if (Clock::now() >= deadline) {
            if (!terminated) {
                std::cerr << "[SandBoxX] Timeout after " << timeout_seconds
                          << "s, sending SIGTERM to " << pid << ".\n";
                stop(pid);
                terminated = true;
                deadline = Clock::now() + std::chrono::seconds(kGraceSeconds);
            } else {
                std::cerr << "[SandBoxX] Process " << pid
                          << " ignored SIGTERM, sending SIGKILL.\n";
                kill(pid, SIGKILL);
                return wait(pid, 0);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool ProcessManager::stop(pid_t pid) {
    return kill(pid, SIGTERM) == 0;
}
}

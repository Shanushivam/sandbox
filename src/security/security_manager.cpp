#include "sandboxx/security_manager.hpp"
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>
#ifdef __linux__
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace sandboxx {
namespace {
bool fail(const char* what) {
    std::cerr << "[SandBoxX] " << what << ": " << std::strerror(errno) << "\n";
    return false;
}

#ifdef __linux__
#if defined(__x86_64__)
constexpr unsigned kAuditArch = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
constexpr unsigned kAuditArch = AUDIT_ARCH_AARCH64;
#endif

#ifdef SECCOMP_RET_KILL_PROCESS
constexpr unsigned kRetKill = SECCOMP_RET_KILL_PROCESS;
#else
constexpr unsigned kRetKill = SECCOMP_RET_KILL;
#endif

std::vector<int> blocked_syscalls() {
    return {
#ifdef __NR_mount
        __NR_mount,
#endif
#ifdef __NR_umount2
        __NR_umount2,
#endif
#ifdef __NR_pivot_root
        __NR_pivot_root,
#endif
#ifdef __NR_chroot
        __NR_chroot,
#endif
#ifdef __NR_fsopen
        __NR_fsopen,
#endif
#ifdef __NR_fsmount
        __NR_fsmount,
#endif
#ifdef __NR_move_mount
        __NR_move_mount,
#endif
#ifdef __NR_open_tree
        __NR_open_tree,
#endif
#ifdef __NR_setns
        __NR_setns,
#endif
#ifdef __NR_unshare
        __NR_unshare,
#endif
#ifdef __NR_ptrace
        __NR_ptrace,
#endif
#ifdef __NR_process_vm_readv
        __NR_process_vm_readv,
#endif
#ifdef __NR_process_vm_writev
        __NR_process_vm_writev,
#endif
#ifdef __NR_reboot
        __NR_reboot,
#endif
#ifdef __NR_kexec_load
        __NR_kexec_load,
#endif
#ifdef __NR_kexec_file_load
        __NR_kexec_file_load,
#endif
#ifdef __NR_init_module
        __NR_init_module,
#endif
#ifdef __NR_finit_module
        __NR_finit_module,
#endif
#ifdef __NR_delete_module
        __NR_delete_module,
#endif
#ifdef __NR_swapon
        __NR_swapon,
#endif
#ifdef __NR_swapoff
        __NR_swapoff,
#endif
#ifdef __NR_bpf
        __NR_bpf,
#endif
#ifdef __NR_perf_event_open
        __NR_perf_event_open,
#endif
#ifdef __NR_userfaultfd
        __NR_userfaultfd,
#endif
#ifdef __NR_keyctl
        __NR_keyctl,
#endif
#ifdef __NR_add_key
        __NR_add_key,
#endif
#ifdef __NR_request_key
        __NR_request_key,
#endif
#ifdef __NR_open_by_handle_at
        __NR_open_by_handle_at,
#endif
#ifdef __NR_acct
        __NR_acct,
#endif
#ifdef __NR_quotactl
        __NR_quotactl,
#endif
#ifdef __NR_syslog
        __NR_syslog,
#endif
#ifdef __NR_sethostname
        __NR_sethostname,
#endif
#ifdef __NR_setdomainname
        __NR_setdomainname,
#endif
#ifdef __NR_settimeofday
        __NR_settimeofday,
#endif
#ifdef __NR_clock_settime
        __NR_clock_settime,
#endif
#ifdef __NR_clock_adjtime
        __NR_clock_adjtime,
#endif
#ifdef __NR_adjtimex
        __NR_adjtimex,
#endif
#ifdef __NR_iopl
        __NR_iopl,
#endif
#ifdef __NR_ioperm
        __NR_ioperm,
#endif
    };
}

bool drop_capabilities() {
    // Emptying the bounding set stops exec from granting root its usual
    // capabilities back; EINVAL marks the end of the kernel's capability list.
    for (int cap = 0; cap < 64; ++cap) {
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) != 0) {
            if (errno == EINVAL) break;
            return fail("drop capability bounding set");
        }
    }
#ifdef PR_CAP_AMBIENT
    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) != 0 && errno != EINVAL)
        return fail("clear ambient capabilities");
#endif
    __user_cap_header_struct header {_LINUX_CAPABILITY_VERSION_3, 0};
    __user_cap_data_struct data[2] {};
    if (syscall(SYS_capset, &header, data) != 0) return fail("capset");
    return true;
}

bool install_seccomp_filter() {
#if defined(__x86_64__) || defined(__aarch64__)
    std::vector<sock_filter> filter = {
        // Kill anything using a different syscall ABI, whose numbers differ.
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, kAuditArch, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, kRetKill),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
    };
#ifdef __x86_64__
    // x32 syscalls share the x86_64 arch value but set this bit.
    filter.push_back(BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 0x40000000, 0, 1));
    filter.push_back(BPF_STMT(BPF_RET | BPF_K, kRetKill));
#endif
    for (int nr : blocked_syscalls()) {
        filter.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, static_cast<unsigned>(nr), 0, 1));
        filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)));
    }
    filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));

    sock_fprog program {static_cast<unsigned short>(filter.size()), filter.data()};
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program, 0, 0) != 0)
        return fail("install seccomp filter");
    return true;
#else
    std::cerr << "[SandBoxX] seccomp filter is not implemented for this CPU architecture.\n";
    return false;
#endif
}
#endif
}

bool SecurityManager::apply() {
#ifdef __linux__
    if (!drop_capabilities()) return false;
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return fail("set no_new_privs");
    return install_seccomp_filter();
#else
    (void)fail;
    std::cerr << "[SandBoxX] Security policy is only supported on Linux.\n";
    return false;
#endif
}
}

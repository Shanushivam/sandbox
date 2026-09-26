#pragma once
namespace sandboxx {
class SecurityManager {
public:
    // Drops all capabilities, sets no_new_privs and installs a seccomp filter
    // that blocks system calls used to escape or tamper with the sandbox.
    // Must be the last setup step before exec.
    bool apply();
};
}

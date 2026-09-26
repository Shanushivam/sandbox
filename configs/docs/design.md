# Design

Core Linux mechanisms used by SandBoxX:
- PID, mount and network namespaces
- cgroup v2 CPU and memory controllers
- mount/chroot based filesystem restriction
- seccomp system-call filtering
- /proc based monitoring
- signal-based safe termination

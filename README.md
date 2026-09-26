# SandBoxX — Linux Process Isolation & Resource Control System

SandBoxX is a small container runtime written in C++17. It runs a command in an
isolated sandbox built directly on Linux kernel primitives: namespaces, cgroup v2,
`pivot_root`, capabilities and seccomp. There are no external dependencies.

```bash
sudo ./build/sandboxx run --config configs/restricted.json /examples/memory_bomb
```

## Features

| Feature | How it works |
|---------|--------------|
| **Process isolation** | New PID, mount, network, UTS and IPC namespaces. The command sees only its own processes, has its own hostname, and gets a network with only loopback. |
| **Init process** | A minimal init runs as PID 1 inside the sandbox. It starts the command, forwards signals (SIGTERM, SIGINT, …) to it and reaps orphaned processes. |
| **Resource limits** | cgroup v2 `cpu.max` and `memory.max`, with swap disabled (where the kernel supports swap accounting) so the limit can't be dodged by swapping. The cgroup is removed when the command finishes. |
| **Restricted filesystem** | `pivot_root` into `rootfs/`, mounted read-only, with a private `/proc`, a writable tmpfs `/tmp` and basic `/dev` nodes (`null`, `zero`, `full`, `random`, `urandom`). The host filesystem is detached completely. |
| **Security policy** | All capabilities dropped, including the bounding set, so root can't regain them. `no_new_privs` is set. A seccomp filter blocks ~40 dangerous system calls (mount, namespace changes, ptrace, kernel modules, kexec, bpf, reboot, …). |
| **Safe termination** | Optional timeout: SIGTERM, then SIGKILL after a 2 second grace period. |
| **Configurable** | JSON config profiles in `configs/`, with every feature switchable. |

## How it works

```text
sandboxx (host, root)
 ├─ create cgroup /sys/fs/cgroup/sandboxx-<pid>   (cpu.max, memory.max)
 └─ clone(CLONE_NEWPID | NEWNS | NEWNET | NEWUTS | NEWIPC)
     └─ init  — PID 1: forwards signals, reaps orphans
         └─ command process — PID 2
             1. join the cgroup
             2. set hostname, make mounts private, bring up loopback
             3. pivot_root into rootfs, remount / read-only, detach host fs
             4. mount a fresh /proc
             5. drop capabilities, set no_new_privs, install seccomp filter
             6. execvp(command)
```

Each step lives in its own manager class (`NamespaceManager`, `CgroupManager`,
`FilesystemManager`, `SecurityManager`, `ProcessManager`), coordinated by
`Runtime`. Isolation is applied inside the child between `fork` and `exec`, so it
confines the command and not SandBoxX itself.

## Requirements

- Linux with namespace support and **cgroup v2** mounted at `/sys/fs/cgroup`
  (the default on current Ubuntu, Debian, Fedora and Arch)
- x86_64 or aarch64 (for the seccomp filter)
- A C++17 compiler and CMake 3.16+
- Root, to run with isolation or limits

On other systems SandBoxX builds, but it refuses to run a command unless every
isolation feature and limit is turned off. It never silently runs a command
unsandboxed.

## Quick start

All commands below are run from the repository root.

```bash
# 1. Build (the binary goes to build/sandboxx, examples to build/examples/)
./scripts/build.sh

# 2. Populate rootfs/ with a shell, a few tools, their libraries and the examples
./scripts/setup_rootfs.sh

# 3. Run a command in the sandbox
sudo ./build/sandboxx run /bin/echo "Hello from SandBoxX"
```

## Usage

```text
sudo ./sandboxx run [--config FILE] [--] <command> [args...]
```

- Arguments are passed straight to the command, with no shell in between, so
  quoting is preserved.
- When `restricted_filesystem` is on, the command path is looked up inside
  `rootfs/`. For example, `/bin/sh` means `rootfs/bin/sh`.
- Use `--` if the command itself starts with `--`.

### Configuration

Without `--config`, the defaults below are used. A config file only needs the
keys it changes, and unknown keys or wrong types are rejected with the error's
position.

| Key | Default | Meaning |
|-----|---------|---------|
| `cpu_percent` | `50` | CPU limit as % of one core; can exceed 100 on multi-core machines (0 = unlimited) |
| `memory_mb` | `128` | Memory limit in MB (0 = unlimited) |
| `pid_namespace` | `true` | Own process tree; the command can't see host processes |
| `mount_namespace` | `true` | Own mount table; required by `restricted_filesystem` |
| `network_namespace` | `true` | No network access except loopback |
| `restricted_filesystem` | `true` | `pivot_root` into `rootfs` |
| `security_policy` | `true` | Drop capabilities and apply the seccomp filter |
| `timeout_seconds` | `0` | Terminate the command after this many seconds (0 = never) |
| `hostname` | `"sandboxx"` | Hostname inside the sandbox |
| `rootfs` | `"./rootfs"` | Root filesystem directory, relative to where you run SandBoxX |

Shipped profiles:

| Profile | CPU | Memory | Network | Filesystem | Security | Timeout |
|---------|-----|--------|---------|------------|----------|---------|
| `configs/default.json` | 50% | 128 MB | isolated | restricted | on | none |
| `configs/development.json` | 80% | 256 MB | host | host | off | none |
| `configs/restricted.json` | 25% | 64 MB | isolated | restricted | on | 30 s |

### Exit codes

SandBoxX exits with the command's own exit code, or `128 + N` if the command was
killed by signal `N` (e.g. `137` when it was OOM-killed, `143` when it was terminated on timeout).
Codes it uses itself:

| Code | Meaning |
|------|---------|
| `2` | Invalid usage or config file |
| `125` | SandBoxX failed before the command ran (e.g. not root, cgroup creation failed) |
| `126` | Sandbox setup failed, or the command is not executable |
| `127` | Command not found |

## Demos

The programs in `examples/` show each kind of isolation. After `setup_rootfs.sh`
they are available inside the sandbox under `/examples/`.

| Command | Expected result |
|---------|-----------------|
| `sudo ./build/sandboxx run /examples/hello` | Prints a greeting |
| `sudo ./build/sandboxx run /examples/hostname_test` | `Hostname: sandboxx`, not the host's name |
| `sudo ./build/sandboxx run /examples/file_test` | `File blocked`, because host `/etc/passwd` is not visible |
| `sudo ./build/sandboxx run /bin/ps` | Only the sandbox's own processes |
| `sudo ./build/sandboxx run --config configs/restricted.json /examples/memory_bomb` | Killed at 64 MB, exit code `137` |
| `sudo ./build/sandboxx run --config configs/restricted.json /examples/cpu_bomb` | Held at 25% CPU (watch with `top`), terminated after 30 s with exit code `143` |

## Testing

```bash
cd build
sudo ctest --output-on-failure
```

| Test | Covers | Needs |
|------|--------|-------|
| `runtime_test` | Exit codes, argument passing, signals, stop, timeout | — |
| `config_test` | JSON parsing, validation, the shipped profiles | — |
| `namespace_test` | PID, hostname and network isolation, init signal forwarding | Linux, root |
| `cgroup_test` | cgroup files, joining the cgroup, OOM kill at the memory limit | Linux, root |
| `filesystem_test` | Host files hidden, read-only root, writable `/tmp`, `/proc` | Linux, root, populated `rootfs/` |
| `security_test` | Capabilities, bounding set, `no_new_privs`, seccomp | Linux, root |

Tests whose requirements aren't met are reported as skipped, not passed.

## Project structure

```text
SandBoxX/
├── CMakeLists.txt
├── include/sandboxx/        # Public headers, one per component
├── src/
│   ├── main.cpp             # Command-line interface
│   ├── runtime/             # Orchestrates the sandbox lifecycle
│   ├── process/             # clone/fork, init process, wait, timeout
│   ├── namespace/           # Namespace flags, hostname, loopback, /proc
│   ├── cgroup/              # cgroup v2 limits
│   ├── filesystem/          # pivot_root and read-only rootfs
│   ├── security/            # Capabilities, no_new_privs, seccomp
│   ├── config/              # JSON config parser and validation
│   └── monitor/             # /proc/<pid>/status inspection
├── configs/                 # Config profiles
├── examples/                # Demo workloads (hello, cpu_bomb, memory_bomb, …)
├── tests/                   # CTest test programs
├── scripts/                 # build.sh, setup_rootfs.sh, cleanup.sh
├── rootfs/                  # Sandbox root filesystem (generated, not committed)
└── docs/                    # Architecture, design, requirements, testing notes
```

## Limitations

- Needs real root; rootless mode (user namespaces) isn't supported yet.
- The network namespace has no external connectivity (no veth or bridge setup).
- The seccomp filter is a denylist, and is only built for x86_64 and aarch64.
- `Monitor` exists but isn't yet wired into the runtime.

## Author

**Shanu Shivam** — [@shanu-shivam](https://github.com/shanu-shivam)

Built on the original SandBoxX skeleton.


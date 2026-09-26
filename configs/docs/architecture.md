# SandBoxX Architecture

SandBoxX is divided into runtime, process, namespace, cgroup, filesystem, security, and monitoring components.

The runtime coordinates the lifecycle. Linux primitives are intentionally isolated behind dedicated managers so each feature can be tested independently.

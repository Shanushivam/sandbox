#include "sandboxx/cgroup_manager.hpp"
#include "test_util.hpp"
#include <fstream>

namespace {
std::string read_line(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    std::getline(file, line);
    return line;
}
}

int main() {
    if (test::skip_unless_linux_root()) return test::kSkip;
    using test::check;
    using test::check_eq;

    sandboxx::CgroupManager cg;
    check(cg.create("sandboxx-test-" + std::to_string(getpid()), 25, 64), "create cgroup");
    std::string path = cg.path();
    check(read_line(path + "/cpu.max") == "25000 100000", "cpu.max set");
    check(read_line(path + "/memory.max") == std::to_string(64L * 1024 * 1024), "memory.max set");
    check(cg.destroy(), "destroy cgroup");
    check(access(path.c_str(), F_OK) != 0, "cgroup directory removed");

    sandboxx::SandboxConfig config = test::bare_config();
    config.memory_mb = 32;
    check_eq(test::run(config, {"sh", "-c", "grep -q sandboxx- /proc/self/cgroup"}), 0,
             "command runs inside the cgroup");
    check_eq(test::run(config, {"sh", "-c",
                                "x=$(head -c 268435456 /dev/zero | tr '\\0' a); echo ${#x}"}),
             128 + 9, "memory limit triggers OOM kill");
    return test::finish();
}

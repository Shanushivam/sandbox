#include "sandboxx/runtime.hpp"
#include "sandboxx/sandbox_config.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace {
int usage() {
    std::cerr << "SandBoxX\n"
                 "Usage: sudo ./sandboxx run [--config FILE] [--] <command> [args...]\n";
    return 2;
}
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) != "run") return usage();

    sandboxx::SandboxConfig config;
    std::string error;
    int i = 2;
    while (i < argc) {
        std::string arg = argv[i];
        std::string path;
        if (arg == "--config") {
            if (i + 1 >= argc) return usage();
            path = argv[i + 1];
            i += 2;
        } else if (arg.rfind("--config=", 0) == 0) {
            path = arg.substr(9);
            ++i;
        } else if (arg == "--") {
            ++i;
            break;
        } else {
            break;
        }
        if (!sandboxx::load_config(path, config, error)) {
            std::cerr << "[SandBoxX] Config error: " << error << "\n";
            return 2;
        }
    }
    if (!sandboxx::validate_config(config, error)) {
        std::cerr << "[SandBoxX] Config error: " << error << "\n";
        return 2;
    }
    if (i >= argc) return usage();

    // Pass arguments through as-is; joining them into a shell string would
    // break quoting and allow shell injection.
    std::vector<std::string> args(argv + i, argv + argc);

    sandboxx::Runtime runtime(config);
    return runtime.run(args);
}

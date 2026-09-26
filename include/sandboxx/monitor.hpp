#pragma once
#include <sys/types.h>
namespace sandboxx {
class Monitor {
public:
    void inspect(pid_t pid);
};
}

#include <unistd.h>
#include <iostream>
int main() {
    char name[256]{};
    if (gethostname(name, sizeof(name)) == 0)
        std::cout << "Hostname: " << name << "\n";
}

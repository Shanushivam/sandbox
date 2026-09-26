#include <fstream>
#include <iostream>
int main() {
    std::ifstream f("/etc/passwd");
    std::cout << (f ? "File accessible\n" : "File blocked\n");
}

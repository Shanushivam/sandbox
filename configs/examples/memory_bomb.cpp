#include <vector>
int main() {
    std::vector<char> data;
    while (true) data.resize(data.size() + 1024 * 1024);
}

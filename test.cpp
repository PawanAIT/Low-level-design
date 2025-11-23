#include <iostream>
#include <functional>
#include <string>

int main() {
    std::hash<int> intHasher;
    std::cout << "Hash of 42: " << intHasher(42) << std::endl;

    std::hash<std::string> stringHasher;
    std::cout << "Hash of 'Hello': " << stringHasher("Hello") << std::endl;

    return 0;
}

#include <iostream>

int main(int argc, char **argv) {
    if (argc > 1)
        std::cout << argv[1] << std::endl;
    else
        std::cout << "Hello, world!" << std::endl;

    return 0;
}

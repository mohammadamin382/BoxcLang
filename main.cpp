
#include "src/compiler/compiler.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        return box::CompilerCLI::run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mUnexpected error:\033[0m " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\033[1;31mUnknown fatal error occurred\033[0m\n";
        return 1;
    }
}

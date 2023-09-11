#include <iostream>

#include "./vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    vm.exec(R"(
        42
    )");

    std::cout << "All done!\n";

    return 0;
}
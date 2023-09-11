#include <iostream>

#include "./vm/Logger.h"
#include "./vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        42
    )");

    log(result.number);

    std::cout << "All done!\n";

    return 0;
}
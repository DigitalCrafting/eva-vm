#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (if (> 5 10) 1 2)
    )");

    log(result);

    std::cout << "All done!\n";

    return 0;
}
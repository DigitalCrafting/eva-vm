#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (var x 3)
        (sum 2 x)
    )");

    log(result);

//    vm.dumpStack();

    std::cout << "All done!\n";

    return 0;
}
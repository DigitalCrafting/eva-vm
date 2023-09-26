#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (var x 5)
        (set x (+ x 10))
        x
        (begin
            (var x 100)
            x
        )
        x
    )");

    log(result);

    std::cout << "All done!\n";

    return 0;
}
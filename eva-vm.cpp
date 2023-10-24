#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (var a 1)
        (var b 2)
        (+ a 1)
        (begin
            (var x 0)
            (var y 20)
            (set x 10)
            (+ x y)
        )
    )");

    log(result);

//    vm.dumpStack();

    std::cout << "All done!\n";

    return 0;
}
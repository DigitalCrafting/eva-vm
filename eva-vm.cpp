#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (var x 10)
        (def foo () x)
        (begin
            (var y 100)
            (set y 1000)
            (var q 300)
            q
            (+ y x)
            (begin
                (var z 200)
                z
                (def bar () (+ y z))
                (bar)
            )
        )
    )");

    log(result);

//    vm.dumpStack();

    std::cout << "All done!\n";

    return 0;
}
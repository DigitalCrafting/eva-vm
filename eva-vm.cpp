#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (def square (x) (* x x))
        (def sum (a b) (begin
            (var x 10)
            (+ x (+ a b))
        ))
//        (square 2)
//        (def factorial (x) (if (== x 1) 1 (* x (factorial (- x 1)))))
//        (factorial 5)
    )");

    log(result);

//    vm.dumpStack();

    std::cout << "All done!\n";

    return 0;
}
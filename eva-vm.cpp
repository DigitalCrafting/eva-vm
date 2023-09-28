#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    EvaVM vm;

    auto result = vm.exec(R"(
        (var count 0)

        (for (var i 0) (< i 10) (set i (+ i 1)) (begin
            (set count (+ count 1))
        ))

        count
    )");

    log(result);

    std::cout << "All done!\n";

    return 0;
}
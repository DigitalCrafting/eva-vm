#include <iostream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    {
        EvaVM vm;
//        Traceable::printStats();
        auto result = vm.exec(R"(
            (+ "Hello" ", world!")
            (+ "Hello" ", world!")
            (+ "Hello" ", world!")
        )");

        log(result);

//        Traceable::printStats();
//      vm.dumpStack();
    }

//    Traceable::printStats();

    std::cout << "All done!\n";
    return 0;
}
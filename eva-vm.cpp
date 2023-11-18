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
            (class Point null
                (def constructor (self x y)
                    (begin
                        //(set (prop self x) x)
                        //(set (prop self y) y)
                        self
                    )
                )
                (def calc (self)
                    //(+ (prop self x) (prop self y))
                    1
                )
            )
            Point
        )");

        log(result);

//        Traceable::printStats();
//      vm.dumpStack();
    }

//    Traceable::printStats();

    std::cout << "All done!\n";
    return 0;
}
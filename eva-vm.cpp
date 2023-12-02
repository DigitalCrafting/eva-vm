#include <iostream>
#include <fstream>

#include "./src/vm/Logger.h"
#include "./src/vm/EvaVM.h"

void printHelp() {
    std::cout << "\nUsage: eva-vm [options]\n\n"
              << "Options:\n"
              << "    -e, --expression  Expression to parse\n"
              << "    -f, --file        File to parse\n\n";
}

/**
 * Eva VM main executable
 * */
int main(int argc, const char *argv[]) {
    if (argc != 3) {
        printHelp();
        return 0;
    }

    /**
   * Expression mode.
   */
    std::string mode = argv[1];

    /**
     * Program to execute.
     */
    std::string program;

    /**
     * Simple expression.
     */
    if (mode == "-e") {
        program = argv[2];
    } else if (mode == "-f") {
        // Read the file:
        std::ifstream programFile(argv[2]);
        std::stringstream buffer;
        buffer << programFile.rdbuf() << "\n";

        // Program:
        program = buffer.str();
    }

    EvaVM vm;
//  Traceable::printStats();
    auto result = vm.exec(program);

    std::cout << "\n";
    log(result);
    std::cout << "\n";

//  Traceable::printStats();
//  vm.dumpStack();

    return 0;
}
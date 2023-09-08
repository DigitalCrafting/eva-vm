#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include "../bytecode/OpCode.h"

#define READ_BYTE() *ip++

/**
 * Eva Virtual Machine
 * */
class EvaVM {
public:
    EvaVM() {}

    /**
     * Execute program
     * */
    void exec(const std::string &program) {
        // 1. Parse the program
        // 2. Compile to Eva bytecode

        code = {OP_HALT};

        // Set instruction pointer to beginning
        ip = &code[0];
        return eval();
    }

    /**
     * Main eval loop.
     * */
     void eval() {
         for (;;) {
            switch(READ_BYTE()) {
                case OP_HALT:
                    return;
            }
         }
     }

    /**
     * Instruction pointer.
     * */
     uint8_t* ip;

    /**
     * Bytecode.
     * */
     std::vector<uint8_t> code;
};

#endif

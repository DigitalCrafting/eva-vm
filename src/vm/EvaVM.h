#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include "./Logger.h"
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
            int opcode = READ_BYTE();
            switch (opcode) {
                case OP_HALT:
                    return;
                default:
                    DIE << "Unknown opcode: " << std::hex << opcode;
            }
        }
    }

    /**
     * Instruction pointer.
     * */
    uint8_t *ip;

    /**
     * Bytecode.
     * */
    std::vector<uint8_t> code;
};

#endif

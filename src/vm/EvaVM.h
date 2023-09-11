#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include <array>
#include "./Logger.h"
#include "./EvaValue.h"
#include "../bytecode/OpCode.h"

#define READ_BYTE() *ip++
#define GET_CONST() constants[READ_BYTE()]
#define STACK_LIMIT 512

/**
 * Eva Virtual Machine
 * */
class EvaVM {
public:
    EvaVM() {
        sp = stack.begin();
    }

    /**
     * Push value onto the stack.
     * */
    void push(const EvaValue& value) {
        if ((size_t)(sp - stack.begin()) == STACK_LIMIT) {
            DIE << "push(): Stack overflow.\n";
        }
        *sp = value;
        sp++;
    }

    /**
     * Pop value from the stack.
     * */
    EvaValue pop() {
         if (sp == stack.begin()) {
             DIE << "pop(): Empty stack.\n";
         }
         --sp;
        return *sp;
     }

    /**
     * Execute program
     * */
    EvaValue exec(const std::string &program) {
        // 1. Parse the program
        // 2. Compile to Eva bytecode

        constants.push_back(NUMBER(42));
        code = {OP_CONST, 0, OP_HALT};

        // Set instruction pointer to beginning
        ip = &code[0];
        return eval();
    }

    /**
     * Main eval loop.
     * */
    EvaValue eval() {
        for (;;) {
            int opcode = READ_BYTE();
            switch (opcode) {
                case OP_HALT:
                    return pop();
                case OP_CONST: {
                    push(GET_CONST());
                    break;
                }
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
     * Stack pointer.
     * */
    EvaValue *sp;

    /**
     * Operands stack.
     * */
    std::array<EvaValue, STACK_LIMIT> stack;

    /**
     * Constant pool.
     * */
    std::vector<EvaValue> constants;

    /**
     * Bytecode.
     * */
    std::vector<uint8_t> code;
};

#endif

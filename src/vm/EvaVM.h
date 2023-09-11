#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include <array>
#include "./Logger.h"
#include "./EvaValue.h"
#include "../bytecode/OpCode.h"

/**
 * Reads the current byte in the bytecode
 * and advances ip pointer.
 * */
#define READ_BYTE() *ip++

/**
 * Gets a constant from the pool.
 * */
#define GET_CONST() constants[READ_BYTE()]

/**
 * Stack top (StackOverflow after exceeding).
 * */
#define STACK_LIMIT 512

/**
 * Binary operation.
 * */
#define BINARY_OP(op) \
    do {              \
        auto op2 = AS_NUMBER(pop()); \
        auto op1 = AS_NUMBER(pop()); \
        push(NUMBER(op1 op op2)); \
    } while (false)

/**
 * Eva Virtual Machine
 * */
class EvaVM {
public:
    EvaVM() {}

    /**
     * Push value onto the stack.
     * */
    void push(const EvaValue &value) {
        if ((size_t) (sp - stack.begin()) == STACK_LIMIT) {
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

        constants.push_back(NUMBER(3));
        constants.push_back(NUMBER(2));
        constants.push_back(NUMBER(5));

        code = {OP_CONST, 0, OP_CONST, 1, OP_ADD, OP_CONST, 2, OP_MUL, OP_HALT};

        // Set instruction pointer to beginning
        ip = &code[0];
        // Init the stack
        sp = &stack[0];

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
                case OP_ADD: {
                    BINARY_OP(+);
                    break;
                }
                case OP_SUB: {
                    BINARY_OP(-);
                    break;
                }
                case OP_MUL: {
                    BINARY_OP(*);
                    break;
                }
                case OP_DIV: {
                    BINARY_OP(/);
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

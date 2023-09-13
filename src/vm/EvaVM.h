#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include <array>
#include "./Logger.h"
#include "./EvaValue.h"
#include "../parser/EvaParser.h"
#include "../compiler/EvaCompiler.h"
#include "../bytecode/OpCode.h"

using syntax::EvaParser;

/**
 * Reads the current byte in the bytecode
 * and advances ip pointer.
 * */
#define READ_BYTE() *ip++

/**
 * Gets a constant from the pool.
 * */
#define GET_CONST() co->constants[READ_BYTE()]

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
 * Generic value comparison.
 * */
#define COMPARE_VALUES(op, v1, v2) \
    do {                           \
        bool res;                  \
        switch (op) {             \
            case 0: {res = v1 < v2; break; }                           \
            case 1: {res = v1 > v2; break; }                           \
            case 2: {res = v1 == v2; break; }                           \
            case 3: {res = v1 >= v2; break; }                           \
            case 4: {res = v1 <= v2; break; }                           \
            case 5: {res = v1 != v2; break; }                           \
        }                          \
        push(BOOLEAN(res));\
    } while (false);

/**
 * Eva Virtual Machine
 * */
class EvaVM {
public:
    EvaVM() :
            parser(std::make_unique<EvaParser>()),
            compiler(std::make_unique<EvaCompiler>()) {}

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
        auto ast = parser->parse(program);

        // 2. Compile to Eva bytecode
        co = compiler->compile(ast);

        // Set instruction pointer to beginning
        ip = &co->code[0];
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
                    auto op2 = pop();
                    auto op1 = pop();
                    if (IS_NUMBER(op1) && IS_NUMBER(op2)) {
                        auto v1 = AS_NUMBER(op1);
                        auto v2 = AS_NUMBER(op2);
                        push(NUMBER(v1 + v2));
                    } else if (IS_STRING(op1) && IS_STRING(op2)) {
                        auto v1 = AS_CPPSTRING(op1);
                        auto v2 = AS_CPPSTRING(op2);
                        push(ALLOC_STRING(v1 + v2));
                    }
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
                case OP_COMPARE: {
                    auto op = READ_BYTE();
                    auto op2 = pop();
                    auto op1 = pop();

                    if (IS_NUMBER(op1) && IS_NUMBER(op2)) {
                        auto v1 = AS_NUMBER(op1);
                        auto v2 = AS_NUMBER(op2);
                        COMPARE_VALUES(op, v1, v2);
                    } else if (IS_STRING(op1) && IS_STRING(op2)) {
                        auto v1 = AS_CPPSTRING(op1);
                        auto v2 = AS_CPPSTRING(op2);
                        COMPARE_VALUES(op, v1, v2);
                    }

                    break;
                }
                default:
                    DIE << "Unknown opcode: " << std::hex << opcode;
            }
        }
    }

    /**
     * Parser
     * */
    std::unique_ptr<EvaParser> parser;

    /**
     * Compiler
     * */
    std::unique_ptr<EvaCompiler> compiler;

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
     * Code object.
     * */
    CodeObject *co;
};

#endif

#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include <string>
#include <vector>
#include <array>
#include "./Logger.h"
#include "./EvaValue.h"
#include "./Global.h"
#include "../compiler/EvaCompiler.h"
#include "../parser/EvaParser.h"
#include "../bytecode/OpCode.h"

using syntax::EvaParser;

/**
 * Reads the current byte in the bytecode
 * and advances ip pointer.
 * */
#define READ_BYTE() *ip++

/**
 * Reads a short word (2 bytes)
 * */
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

/**
 * Converts bytecode index to pointer
 * */
#define TO_ADDRESS(index) (&co->code[index])

/**
 * Gets a constant from the pool.
 * */
#define GET_CONST() (co->constants[READ_BYTE()])

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
            globals(std::make_shared<Global>()),
            parser(std::make_unique<EvaParser>()),
            compiler(std::make_unique<EvaCompiler>(globals)) {
        setGlobalVariables();
    }

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
            DIE << "pop(): empty stack.\n";
        }
        --sp;
        return *sp;
    }

    /**
     * Peek an element from the stack.
     * */
    EvaValue peek(size_t offset = 0) {
        if (stack.size() == 0) {
            DIE << "peek(): empty stack.\n";
        }
        return *(sp - 1 - offset);
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

        compiler->disassembleBytecode();

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
                case OP_JMP_IF_FALSE: {
                    auto cond = AS_BOOLEAN(pop());
                    auto address = READ_SHORT();
                    if (!cond) {
                        ip = TO_ADDRESS(address);
                    }
                    break;
                }
                case OP_JMP: {
                    auto address = READ_SHORT();
                    ip = TO_ADDRESS(address);
                    break;
                }
                case OP_GET_GLOBAL: {
                    auto globalIndex = (int) READ_BYTE();
                    push(globals->get(globalIndex).value);
                    break;
                }
                case OP_SET_GLOBAL: {
                    auto globalIndex = (int) READ_BYTE();
                    auto value = peek(0);
                    globals->set(globalIndex, value);
                    break;
                }
                default:
                    DIE << "Unknown opcode: " << std::hex << opcode;
            }
        }
    }

    /**
     * Sets up global variables and functions.
     * */
    void setGlobalVariables() {
        globals->addConst("x", 10);
        globals->addConst("y", 20);
    }

    /**
     * Global object
     * */
    std::shared_ptr<Global> globals;

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

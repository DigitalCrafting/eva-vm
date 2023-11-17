#ifndef EVA_VM_EVAVM_H
#define EVA_VM_EVAVM_H

#include "../bytecode/OpCode.h"
#include "../compiler/EvaCompiler.h"
#include "../gc/EvaCollector.h"
#include "../parser/EvaParser.h"
#include "./EvaValue.h"
#include "./Global.h"
#include "./Logger.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

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
#define TO_ADDRESS(index) (&fn->co->code[index])

/**
 * Gets a constant from the pool.
 * */
#define GET_CONST() (fn->co->constants[READ_BYTE()])

/**
 * Stack top (StackOverflow after exceeding).
 * */
#define STACK_LIMIT 512

/**
 * Memory threshold after which GC is triggered.
 * */
#define GC_THRESHOLD 1024

/**
 * Runtime allocation, can call GC.
 * */
#define MEM(allocator, ...) (maybeGC(), allocator(__VA_ARGS__))

/**
 * Binary operation.
 * */
#define BINARY_OP(op)                                                          \
  do {                                                                         \
    auto op2 = AS_NUMBER(pop());                                               \
    auto op1 = AS_NUMBER(pop());                                               \
    push(NUMBER(op1 op op2));                                                  \
  } while (false)

/**
 * Generic value comparison.
 * */
#define COMPARE_VALUES(op, v1, v2)                                             \
  do {                                                                         \
    bool res;                                                                  \
    switch (op) {                                                              \
    case 0: {                                                                  \
      res = v1 < v2;                                                           \
      break;                                                                   \
    }                                                                          \
    case 1: {                                                                  \
      res = v1 > v2;                                                           \
      break;                                                                   \
    }                                                                          \
    case 2: {                                                                  \
      res = v1 == v2;                                                          \
      break;                                                                   \
    }                                                                          \
    case 3: {                                                                  \
      res = v1 >= v2;                                                          \
      break;                                                                   \
    }                                                                          \
    case 4: {                                                                  \
      res = v1 <= v2;                                                          \
      break;                                                                   \
    }                                                                          \
    case 5: {                                                                  \
      res = v1 != v2;                                                          \
      break;                                                                   \
    }                                                                          \
    }                                                                          \
    push(BOOLEAN(res));                                                        \
  } while (false);

/**
 * Stack frame for function calls.
 * */
struct Frame {
    /**
     * Return address of the caller.
     * */
    uint8_t *ra;

    /**
     * Base pointer of the caller.
     * */
    EvaValue *bp;

    /**
     * Reference to the running function:
     * contains code, locals, etc.
     * */
    FunctionObject *fn;
};

/**
 * Eva Virtual Machine
 * */
class EvaVM {
public:
    EvaVM()
            : globals(std::make_shared<Global>()),
              parser(std::make_unique<EvaParser>()),
              compiler(std::make_unique<EvaCompiler>(globals)),
              collector(std::make_unique<EvaCollector>()) {
        setGlobalVariables();
    }

    /* VM shutdown */
    ~EvaVM() { Traceable::cleanup(); }

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
        auto ast = parser->parse("(begin " + program + ")");

        // 2. Compile to Eva bytecode
        compiler->compile(ast);

        fn = compiler->getMainFunction();

        // Set instruction pointer to beginning
        ip = &fn->co->code[0];
        // Init the stack
        sp = &stack[0];
        // Init the base (frame) pointer
        bp = sp;

        compiler->disassembleBytecode();

        return eval();
    }

    /**
     * Main eval loop.
     * */
    EvaValue eval() {
        for (;;) {
            //            dumpStack();
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
                        push(MEM(ALLOC_STRING, v1 + v2));
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
                    auto value = pop();
                    globals->set(globalIndex, value);
                    break;
                }
                    // Stack manipulation
                case OP_POP: {
                    pop();
                    break;
                }
                case OP_GET_LOCAL: {
                    auto localIndex = READ_BYTE();
                    if (localIndex < 0 || localIndex >= stack.size()) {
                        DIE << "OP_GET_LOCAL: invalid variable index: " << (int) localIndex;
                    }
                    push(bp[localIndex]);
                    break;
                }
                case OP_SET_LOCAL: {
                    auto localIndex = READ_BYTE();
                    auto value = peek(0);
                    if (localIndex < 0 || localIndex >= stack.size()) {
                        DIE << "OP_SET_LOCAL: invalid variable index: " << (int) localIndex;
                    }
                    bp[localIndex] = value;
                    break;
                }
                case OP_GET_CELL: {
                    auto cellIndex = READ_BYTE();
                    push(fn->cells[cellIndex]->value);
                    break;
                }
                case OP_SET_CELL: {
                    auto cellIndex = READ_BYTE();
                    auto value = peek(0);

                    // Allocate the cell if it's not there yet
                    if (fn->cells.size() <= cellIndex) {
                        fn->cells.push_back(AS_CELL(MEM(ALLOC_CELL, value)));
                    } else {
                        // Update the cell
                        fn->cells[cellIndex]->value = value;
                    }

                    break;
                }
                case OP_LOAD_CELL: {
                    auto cellIndex = READ_BYTE();
                    push(CELL(fn->cells[cellIndex]));
                    break;
                }
                case OP_MAKE_FUNCTION: {
                    auto co = AS_CODE(pop());
                    auto cellsCount = READ_BYTE();

                    auto fnValue = MEM(ALLOC_FUNCTION, co);
                    auto fn = AS_FUNCTION(fnValue);

                    // Capture
                    for (auto i = 0; i < cellsCount; i++) {
                        fn->cells.push_back(AS_CELL(pop()));
                    }

                    push(fnValue);
                    break;
                }
                    /**
                     * Clean up variables:
                     *
                     * Note: variables sit right below the result of a block,
                     * so we move the result below, which will be new top
                     * after popping the variables.
                     * */
                case OP_SCOPE_EXIT: {
                    auto count = READ_BYTE();

                    // Move the result above (or below, depends on the visualization) the
                    // vars
                    *(sp - 1 - count) = peek(0);

                    popN(count);

                    break;
                }
                    /**
                     * Function calls
                     * */
                case OP_CALL: {
                    auto argsCount = READ_BYTE();
                    auto fnValue = peek(argsCount);

                    // 1. Native function
                    if (IS_NATIVE(fnValue)) {
                        AS_NATIVE(fnValue)->function();
                        auto result = pop();
                        popN(argsCount + 1);
                        push(result);
                        break;
                    }

                    // 2. User-defined function
                    auto callee = AS_FUNCTION(fnValue);

                    callStack.push(Frame{ip, bp, fn});

                    // To access locals, etc:
                    fn = callee;

                    // Shrink the cells vector to the size of only free vars, since other
                    // (own) cells should be reallocated for each invocation
                    fn->cells.resize(fn->co->freeCount);

                    // Set the base (frame) pointer for the callee
                    bp = sp - argsCount - 1;

                    ip = &callee->co->code[0];

                    break;
                }
                    /* Return from function */
                case OP_RETURN: {
                    auto callerFrame = callStack.top();
                    ip = callerFrame.ra;
                    bp = callerFrame.bp;
                    fn = callerFrame.fn;

                    callStack.pop();
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
        /* Native functions */
        globals->addNativeFunction(
                "native-square",
                [&]() {
                    auto x = AS_NUMBER(peek(0));
                    push(NUMBER(x * x));
                },
                1);

        globals->addNativeFunction(
                "native-sum",
                [&]() {
                    // Remember: first we take out second arg, because of stack structure
                    auto v2 = AS_NUMBER(peek(0));
                    auto v1 = AS_NUMBER(peek(1));
                    push(NUMBER(v1 + v2));
                },
                2);

        /* Global variables */
        globals->addConst("VERSION", 1);
        globals->addConst("y", 20);
    }

    void popN(size_t count) {
        if (stack.size() == 0) {
            DIE << "popN(): empty stack.\n";
        }
        sp -= count;
    }

    // ----------------------------------------------
    // GC Operations:

    std::set<Traceable *> getStackGCRoots() {
        std::set<Traceable *> roots;
        auto stackEntry = sp;
        while (stackEntry-- != stack.begin()) {
            if (IS_OBJECT(*stackEntry)) {
                roots.insert((Traceable *) stackEntry->object);
            }
        }
        return roots;
    }

    std::set<Traceable *> getConstantGCRoots() {
        return compiler->getConstantObjects();
    }

    std::set<Traceable *> getGlobalGCRoots() {
        std::set<Traceable *> roots;
        for (const auto& global : globals->globals) {
            if (IS_OBJECT(global.value)) {
                roots.insert((Traceable*)global.value.object);
            }
        }
        return roots;
    }

    /**
     * Obtain GC roots: variables on the stack, globals, constants.
     * */
    std::set<Traceable *> getGCRoots() {
        // Stack:
        auto roots = getStackGCRoots();

        // Constant pool
        auto constantRoots = getConstantGCRoots();
        roots.insert(constantRoots.begin(), constantRoots.end());

        // Global
        auto globalRoots = getGlobalGCRoots();
        roots.insert(globalRoots.begin(), globalRoots.end());

        return roots;
    }

    /**
     * Spawns a pottential GC cycle.
     * */
    void maybeGC() {
        if (Traceable::bytesAllocated < GC_THRESHOLD) {
            return;
        }

        auto roots = getGCRoots();

        if (roots.size() == 0) {
            return;
        }

        std::cout << "---------------- Before GC stats ----------------\n";
        Traceable::printStats();

        collector->gc(roots);

        std::cout << "---------------- After GC stats ----------------\n";
        Traceable::printStats();
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
     * Garbage collector
     * */
    std::unique_ptr<EvaCollector> collector;

    /**
     * Instruction pointer.
     * */
    uint8_t *ip;

    /**
     * Stack pointer.
     * */
    EvaValue *sp;

    /**
     * Base pointer (aka Frame pointer).
     * */
    EvaValue *bp;

    /**
     * Operands stack.
     * */
    std::array<EvaValue, STACK_LIMIT> stack;

    /**
     * Separate stack for the calls. Keeps return address.
     * */
    std::stack<Frame> callStack;

    /**
     * Currently executing function.
     * */
    FunctionObject *fn;

    /**
     * Dumps the current stack
     * */
    void dumpStack() {
        std::cout << "\n---------- Stack ----------\n";
        if (sp == stack.begin()) {
            std::cout << "(empty)";
        }

        auto csp = sp - 1;
        while (csp >= stack.begin()) {
            std::cout << *csp-- << "\n";
        }
        std::cout << "\n";
    }
};

#endif

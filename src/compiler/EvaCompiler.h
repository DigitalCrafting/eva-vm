#ifndef EVA_VM_EVACOMPILER_H
#define EVA_VM_EVACOMPILER_H

#include <vector>
#include <map>
#include <string>
#include "../disassembler/EvaDisassembler.h"
#include "../parser/EvaParser.h"
#include "../vm/EvaValue.h"
#include "../vm/Logger.h"
#include "../vm/Global.h"
#include "../bytecode/OpCode.h"

#define ALLOC_CONST(tester, converter, allocator, value)    \
    do {                                                    \
        for (auto i = 0; i < co->constants.size(); i++) {   \
            if (!tester(co->constants[i])) {                \
            continue;                                       \
            }                                               \
            if (converter(co->constants[i]) == value) {     \
                return i;                                   \
            }                                               \
        }                                                   \
        co->constants.push_back(allocator(value));          \
    } while (false);


// Generic binary operation: (+ 1 2) OP_CONST, OP_CONST, OP_ADD
#define GEN_BINARY_OP(op) \
    do { \
        gen(exp.list[1]);          \
        gen(exp.list[2]);          \
        emit(op);                  \
    } while (false);

class EvaCompiler {
public:
    explicit EvaCompiler(std::shared_ptr<Global> globals) :
            globals(globals),
            disassembler(std::make_unique<EvaDisassembler>(globals)) {
    }

    CodeObject *compile(const Exp &exp) {
        // Allocate new code object:
        co = AS_CODE(ALLOC_CODE("main"));
        // Generate recursively from top-level:
        gen(exp);
        // Explicit VM-stop marker.
        emit(OP_HALT);

        return co;
    }

    /**
     * Main compile loop.
     * */
    void gen(const Exp &exp) {
        switch (exp.type) {
            case ExpType::NUMBER: {
                emit(OP_CONST);
                emit(numericConstIdx(exp.number));
                break;
            }
            case ExpType::STRING: {
                emit(OP_CONST);
                emit(stringConstIdx(exp.string));
                break;
            }
            case ExpType::SYMBOL: {
                /* Boolean */
                if (exp.string == "true" || exp.string == "false") {
                    emit(OP_CONST);
                    emit(booleanConstIdx(exp.string == "true"));
                } else {
                    /* Variable */
                    auto varName = exp.string;

                    auto localIndex = co->getLocalIndex(varName);
                    if (localIndex != -1) {
                        // 1. Local variables
                        emit(OP_GET_LOCAL);
                        emit(localIndex);
                    } else {
                        // 2. Global variables
                        if (!globals->exists(varName)) {
                            DIE << "[EvaCompiler]: Reference error: " << varName << " doesn't exist.";
                        }

                        emit(OP_GET_GLOBAL);
                        emit(globals->getGlobalIndex(varName));
                    }
                }
                break;
            }
            case ExpType::LIST: {
                auto tag = exp.list[0];

                /**
                 * Special cases.
                 * */
                if (tag.type == ExpType::SYMBOL) {
                    auto op = tag.string;

                    /* Binary math operations */
                    if (op == "+") {
                        GEN_BINARY_OP(OP_ADD);
                    } else if (op == "-") {
                        GEN_BINARY_OP(OP_SUB);
                    } else if (op == "*") {
                        GEN_BINARY_OP(OP_MUL);
                    } else if (op == "/") {
                        GEN_BINARY_OP(OP_DIV);
                    }
                        /* Compare operations */
                    else if (compareOps_.count(op) != 0) {
                        gen(exp.list[1]);
                        gen(exp.list[2]);
                        emit(OP_COMPARE);
                        emit(compareOps_[op]);
                    }
                        /* Branch instruction */
                        /* if <test> <consequent> <alternate> */
                    else if (op == "if") {
                        gen(exp.list[1]);
                        emit(OP_JMP_IF_FALSE);

                        // Else branch. Init with 0 address, will be patched.
                        // Note: we use 2-byte addresses
                        emit(0);
                        emit(0);

                        auto elseJmpAddress = getOffset() - 2;

                        // Emit <consequent>:
                        gen(exp.list[2]);
                        emit(OP_JMP);

                        // 2-byte address
                        emit(0);
                        emit(0);

                        auto endAddress = getOffset() - 2;

                        // Patch the else branch address.
                        auto elseBranchAddress = getOffset();
                        patchJumpAddress(elseJmpAddress, elseBranchAddress);

                        if (exp.list.size() == 4) {
                            gen(exp.list[3]);
                        }

                        // Path the end
                        auto endBranchAddr = getOffset();
                        patchJumpAddress(endAddress, endBranchAddr);
                    }
                        /* While loop */
                    else if (op == "while") {
                        auto loopStartAddress = getOffset();
                        // Emit test
                        gen(exp.list[1]);
                        emit(OP_JMP_IF_FALSE);

                        emit(0);
                        emit(0);

                        auto loopEndJmpAddress = getOffset() - 2;
                        // Emit body
                        gen(exp.list[2]);

                        emit(OP_JMP);

                        emit(0);
                        emit(0);

                        patchJumpAddress(getOffset() - 2, loopStartAddress);

                        // Patch the end
                        auto loopEndAddr = getOffset() + 1;
                        patchJumpAddress(loopEndJmpAddress, loopEndAddr);
                    }
                        /* For loop */
                    else if (op == "for") {

                        // Initialize variable
                        gen(exp.list[1]);

                        std::string whileSymbolLiteral = "while";
                        Exp whileSymbolObj(whileSymbolLiteral);

                        Exp whileBody(exp.list[4]);
                        whileBody.list.push_back(exp.list[3]);

                        gen(Exp({whileSymbolObj, exp.list[2], whileBody}));
                    }
                        /* Variable declaration */
                    else if (op == "var") {
                        auto varName = exp.list[1].string;
                        gen(exp.list[2]);

                        if (isGlobalScope()) {
                            // 1. Global vars
                            globals->define(varName);
                            // Initializer:
                            emit(OP_SET_GLOBAL);
                            emit(globals->getGlobalIndex(varName));
                        } else {
                            // 2. Local vars
                            co->addLocal(varName);
                            emit(OP_SET_LOCAL);
                            emit(co->getLocalIndex(varName));
                        }
                    } else if (op == "set") {
                        auto varName = exp.list[1].string;
                        gen(exp.list[2]);

                        auto localIndex = co->getLocalIndex(varName);
                        if (localIndex != -1) {
                            // 1. Local vars
                            emit(OP_SET_LOCAL);
                            emit(localIndex);
                        } else {
                            // 2. Global vars
                            auto globalIndex = globals->getGlobalIndex(varName);

                            if (globalIndex == -1) {
                                DIE << "Reference error: " << varName << " is not defined.";
                            }
                            // Initializer:
                            emit(OP_SET_GLOBAL);
                            emit(globalIndex);
                        }
                    } else if (op == "begin") {
                        scopeEnter();

                        // Compile each expression within a block
                        for (auto i = 1; i < exp.list.size(); i++) {
                            // The value of the last expression is kept
                            // on the stack as the final result.
                            bool isLast = i == exp.list.size() - 1;

                            auto isLocalDeclaration = isDeclaration(exp.list[i]) && !isGlobalScope();

                            // Generate expression code
                            gen(exp.list[i]);

                            if (!isLast && !isLocalDeclaration) {
                                emit(OP_POP);
                            }
                        }

                        scopeExit();
                    }
                }
                break;
            }
        }
    }

    /**
     * Disassemble all compilation units.
     * */
    void disassembleBytecode() {
        disassembler->disassemble(co);
    }

private:
    /**
     * Global object.
     * */
    std::shared_ptr<Global> globals;

    /**
     * Disassembler.
     * */
    std::unique_ptr<EvaDisassembler> disassembler;

    /**
     * Enters a new scope.
     * */
    void scopeEnter() {
        co->scopeLevel++;
    }

    /**
     * Exists a new scope.
     * */
    void scopeExit() {
        // Pop vars from the stack if they were declared
        // within this specific scope.
        auto varsCount = getVarsCountOnScopeExit();

        if (varsCount > 0) {
            emit(OP_SCOPE_EXIT);
            emit(varsCount);
        }

        co->scopeLevel--;
    }

    /**
     * Whether it's the global scope.
     * */
    bool isGlobalScope() {
        return co->name == "main" && co->scopeLevel == 1; // We have implicit block 0, so global scope will be 1
    }

    /**
     * Whether the expression is a decalration.
     * */
    bool isDeclaration(const Exp &exp) { return isVarDeclaration(exp); }

    /**
     * (var <name> <value>)
     * */
    bool isVarDeclaration(const Exp &exp) {
        return isTaggedList(exp, "var");
    }

    /**
     * Tagged lists.
     * */
    bool isTaggedList(const Exp &exp, const std::string &tag) {
        return exp.type == ExpType::LIST && exp.list[0].type == ExpType::SYMBOL && exp.list[0].string == tag;
    }

    size_t getVarsCountOnScopeExit() {
        auto varsCount = 0;

        if (co->locals.size() > 0) {
            while (co->locals.back().scopeLevel == co->scopeLevel) {
                co->locals.pop_back();
                varsCount++;
            }
        }

        return varsCount;
    }

    /**
     * Returns current bytecode offset.
     * */
    size_t getOffset() { return co->code.size(); }

    /**
     * Allocates a numeric constant.
     * */
    size_t numericConstIdx(double value) {
        ALLOC_CONST(IS_NUMBER, AS_NUMBER, NUMBER, value);
        return co->constants.size() - 1;
    }

    /**
     * Allocates a string constant.
     * */
    size_t stringConstIdx(const std::string &value) {
        ALLOC_CONST(IS_STRING, AS_CPPSTRING, ALLOC_STRING, value);
        return co->constants.size() - 1;
    }

    /**
     * Allocates a boolean constant.
     * */
    size_t booleanConstIdx(const bool value) {
        ALLOC_CONST(IS_BOOLEAN, AS_BOOLEAN, BOOLEAN, value);
        return co->constants.size() - 1;
    }

    /**
     * Emits data to the bytecode.
     * */
    void emit(uint8_t code) {
        co->code.push_back(code);
    }

    /**
     * Writes byte at offset.
     * */
    void writeByteAtOffset(size_t offset, uint8_t value) {
        co->code[offset] = value;
    }

    /**
     * Patches jump address.
     * */
    void patchJumpAddress(size_t offset, uint16_t value) {
        writeByteAtOffset(offset, (value >> 8) & 0xff);
        writeByteAtOffset(offset + 1, value & 0xff);
    }

    CodeObject *co{};

    /**
     * Compare ops map.
     * */
    static std::map<std::string, uint8_t> compareOps_;
};

/**
 * Compare ops map.
 * */
std::map<std::string, uint8_t> EvaCompiler::compareOps_ = {
        {"<",  0},
        {">",  1},
        {"==", 2},
        {">=", 3},
        {"<=", 4},
        {"!=", 5}
};

#endif

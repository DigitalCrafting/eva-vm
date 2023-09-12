#ifndef EVA_VM_EVACOMPILER_H
#define EVA_VM_EVACOMPILER_H

#include <vector>
#include "../parser/EvaParser.h"
#include "../vm/EvaValue.h"
#include "../vm/Logger.h"
#include "../bytecode/OpCode.h"

class EvaCompiler {
public:
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
                DIE << "ExpType::SYMBOL: unimplemented.";
                break;
            }
            case ExpType::LIST: {
                DIE << "ExpType::LIST: unimplemented.";
                break;
            }
        }
    }

private:
    /**
     * Allocates a numeric constant.
     * */
     size_t numericConstIdx(double value) {
        for (auto i = 0; i < co->constants.size(); i++) {
            if (!IS_NUMBER(co->constants[i])) {
                continue;
            }

            if (AS_NUMBER(co->constants[i]) == value) {
                return i;
            }
        }
        co->constants.push_back(NUMBER(value));
        return co->constants.size() - 1;
     }

    /**
     * Allocates a string constant.
     * */
     size_t stringConstIdx(const std::string& value) {
        for (auto i = 0; i < co->constants.size(); i++) {
            if (!IS_STRING(co->constants[i])) {
                continue;
            }

            if (AS_CPPSTRING(co->constants[i]) == value) {
                return i;
            }
        }
        co->constants.push_back(ALLOC_STRING(value));
        return co->constants.size() - 1;
     }

    /**
     * Emits data to the bytecode.
     * */
    void emit(uint8_t code) {
        co->code.push_back(code);
    }

    CodeObject *co;
};

#endif

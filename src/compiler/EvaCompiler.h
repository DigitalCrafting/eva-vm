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
#include "../vm/Scope.h"
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
        co->addConstant(allocator(value));          \
    } while (false);


// Generic binary operation: (+ 1 2) OP_CONST, OP_CONST, OP_ADD
#define GEN_BINARY_OP(op) \
    do { \
        gen(exp.list[1]);          \
        gen(exp.list[2]);          \
        emit(op);                  \
    } while (false);

#define FUNCTION_CALL(exp) \
    do {                   \
        gen(exp.list[0]); \
        for (auto i = 1; i < exp.list.size(); i++) { \
            gen(exp.list[i]); \
        } \
        emit(OP_CALL); \
        emit(exp.list.size() - 1); \
    } while (false);


class EvaCompiler {
public:
    explicit EvaCompiler(std::shared_ptr<Global> globals) :
            globals(globals),
            disassembler(std::make_unique<EvaDisassembler>(globals)) {
    }

    void compile(const Exp &exp) {
        // Allocate new code object:
        co = AS_CODE(createCodeObjectValue("main"));
        main = AS_FUNCTION(ALLOC_FUNCTION(co));

        constantObject_.insert((Traceable *) main);

        // Scope analysis
        analyze(exp, nullptr);

        // Generate recursively from top-level:
        gen(exp);
        // Explicit VM-stop marker.
        emit(OP_HALT);
    }

    /**
     * Scope analysis
     * */
    void analyze(const Exp &exp, std::shared_ptr<Scope> scope) {
        if (exp.type == ExpType::SYMBOL) {
            /**
             * Boolean
             * */
            if (exp.string == "true" || exp.string == "false" || exp.string == "null") {
                // Do nothing
            } else {
                /**
                 * Variables
                 * */
                scope->maybePromote(exp.string);
            }
        } else if (exp.type == ExpType::LIST) {
            auto tag = exp.list[0];

            if (tag.type == ExpType::SYMBOL) {
                auto op = tag.string;

                if (op == "begin") {
                    auto newScope = std::make_shared<Scope>(
                            scope == nullptr ? ScopeType::GLOBAL : ScopeType::BLOCK,
                            scope
                    );

                    scopeInfo_[&exp] = newScope;

                    for (auto i = 1; i < exp.list.size(); ++i) {
                        analyze(exp.list[i], newScope);
                    }
                }
                    // Variable declaration
                else if (op == "var") {
                    scope->addLocal(exp.list[1].string);
                    analyze(exp.list[2], scope);
                }
                    // Function declaration
                else if (op == "def") {
                    auto fnName = exp.list[1].string;

                    scope->addLocal(fnName);

                    auto newScope = std::make_shared<Scope>(ScopeType::FUNCTION, scope);
                    scopeInfo_[&exp] = newScope;

                    newScope->addLocal(
                            fnName); // we add function name as it's own local variable in order to do recursive calls

                    auto arity = exp.list[2].list.size();

                    // Params
                    for (auto i = 0; i < arity; i++) {
                        newScope->addLocal(exp.list[2].list[i].string);
                    }

                    // Body
                    analyze(exp.list[3], newScope);
                }
                    // Lambda
                else if (op == "lambda") {
                    auto newScope = std::make_shared<Scope>(ScopeType::FUNCTION, scope);
                    scopeInfo_[&exp] = newScope;
                    auto arity = exp.list[1].list.size();

                    // Params
                    for (auto i = 0; i < arity; i++) {
                        newScope->addLocal(exp.list[1].list[i].string);
                    }

                    // Body
                    analyze(exp.list[2], newScope);
                }
                    // Class declaration
                else if (op == "class") {
                    auto className = exp.list[1].string;
                    auto newScope = std::make_shared<Scope>(ScopeType::CLASS, scope);
                    scopeInfo_[&exp] = newScope;
                    scope->addLocal(className);
                    for (auto i = 3; i < exp.list.size(); i++) {
                        analyze(exp.list[i], scope);
                    }
                } else {
                    for (auto i = 1; i < exp.list.size(); ++i) {
                        analyze(exp.list[i], scope);
                    }
                }
            } else {
                for (auto i = 0; i < exp.list.size(); ++i) {
                    analyze(exp.list[i], scope);
                }
            }
        }
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

                    auto opCodeGetter = scopeStack_.top()->getNameGetter(varName);
                    emit(opCodeGetter);

                    if (opCodeGetter == OP_GET_LOCAL) {
                        // 1. Local variables
                        emit(co->getLocalIndex(varName));
                    } else if (opCodeGetter == OP_GET_CELL) {
                        emit(co->getCellIndex(varName));
                    } else {
                        // 2. Global variables
                        if (!globals->exists(varName)) {
                            DIE << "[EvaCompiler]: Reference error: " << varName << " doesn't exist.";
                        }

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
                        auto opCodeSetter = scopeStack_.top()->getNameSetter(varName);

                        // Special treatment of (var foo (lambda ...))
                        if (isLambda(exp.list[2])) {
                            compileFunction(
                                    exp.list[2],
                                    varName,
                                    exp.list[2].list[1],
                                    exp.list[2].list[2]
                            );
                        } else {
                            gen(exp.list[2]);
                        }

                        if (opCodeSetter == OP_SET_GLOBAL) {
                            // 1. Global vars
                            globals->define(varName);
                            // Initializer:
                            emit(OP_SET_GLOBAL);
                            emit(globals->getGlobalIndex(varName));
                        } else if (opCodeSetter == OP_SET_CELL) {
                            // 2. Cells
                            co->cellNames.push_back(varName);
                            emit(OP_SET_CELL);
                            emit(co->cellNames.size() - 1);
                            emit(OP_POP);
                        } else {
                            // 3. Local vars
                            co->addLocal(varName);
                        }
                    } else if (op == "set") {
                        auto varName = exp.list[1].string;
                        auto opCodeSetter = scopeStack_.top()->getNameSetter(varName);

                        gen(exp.list[2]);

                        if (opCodeSetter == OP_SET_LOCAL) {
                            // 1. Local vars
                            emit(OP_SET_LOCAL);
                            emit(co->getLocalIndex(varName));
                        } else if (opCodeSetter == OP_SET_CELL) {
                            // 2. Cell vars
                            emit(OP_SET_CELL);
                            emit(co->getCellIndex(varName));
                        } else {
                            // 3. Global vars
                            auto globalIndex = globals->getGlobalIndex(varName);

                            if (globalIndex == -1) {
                                DIE << "Reference error: " << varName << " is not defined.";
                            }
                            // Initializer:
                            emit(OP_SET_GLOBAL);
                            emit(globalIndex);
                        }
                    } else if (op == "begin") {
                        scopeStack_.push(scopeInfo_.at(&exp));
                        blockEnter();

                        // Compile each expression within a block
                        for (auto i = 1; i < exp.list.size(); i++) {
                            // The value of the last expression is kept
                            // on the stack as the final result.
                            bool isLast = (i == exp.list.size() - 1);

                            auto isDecl = isDeclaration(exp.list[i]);

                            // Generate expression code
                            gen(exp.list[i]);

                            if (!isLast && !isDecl) {
                                emit(OP_POP);
                            }
                        }

                        blockExit();
                        scopeStack_.pop();
                    }
                        /**
                         * Function declaration: (def <name> <params> <body>)
                         *
                         * Sugar for: (var <name> (lambda <params> <body>))
                         */
                    else if (op == "def") {
                        auto fnName = exp.list[1].string;

                        compileFunction(
                                exp,
                                fnName,
                                exp.list[2],
                                exp.list[3]);

                        if (classObject_ == nullptr) {
                            if (isGlobalScope()) {
                                globals->define(fnName);
                                emit(OP_SET_GLOBAL);
                                emit(globals->getGlobalIndex(fnName));
                            } else {
                                co->addLocal(fnName);
                                // Note: no need to explicitly "set" the var value, since the
                                // function is already on the stack in the correct slot.
                            }
                        }
                    }
                        /**
                         * Lambda: (lambda <params> <body>)
                         */
                    else if (op == "lambda") {
                        compileFunction(
                                exp,
                                "lambda",
                                exp.list[1],
                                exp.list[2]);
                    }
                        /**
                         * Class declaration:
                         *
                         * (class A <super> <body>)
                         * */
                    else if (op == "class") {
                        auto name = exp.list[1].string;
                        auto superClass = exp.list[2].string == "null"
                                          ? nullptr
                                          : getClassByName(exp.list[2].string);

                        auto cls = ALLOC_CLASS(name, superClass);
                        auto classObject = AS_CLASS(cls);

                        classObjects_.push_back(classObject);

                        // Track for GC
                        constantObject_.insert((Traceable *) classObject);

                        // Put the class in constant pool
                        co->addConstant(cls);

                        // Register as global
                        globals->define(name);
                        // And pre-install to the globals
                        globals->set(globals->getGlobalIndex(name), cls);

                        // To compile class body we set the current
                        // compiling class, so the defined methods are
                        // stored in the class.
                        if (exp.list.size() > 3) {
                            auto prevClassObject = classObject_;
                            classObject_ = classObject;

                            // Body:
                            scopeStack_.push(scopeInfo_.at(&exp));
                            for (auto i = 3; i < exp.list.size(); i++) {
                                gen(exp.list[i]);
                            }
                            scopeStack_.pop();
                            classObject_ = prevClassObject;
                        }

                    }
                        /* Function calls */
                    else {
                        FUNCTION_CALL(exp);
                    }
                }
                    /* Lambda function calls */
                else {
                    FUNCTION_CALL(exp);
                }
                break;
            }
        }
    }

    void compileFunction(const Exp &exp, const std::string fnName, const Exp &paramsExp, const Exp &body) {
        auto scopeInfo = scopeInfo_.at(&exp);
        scopeStack_.push(scopeInfo);

        auto params = paramsExp.list;
        auto arity = params.size();

        // Save previous code object
        auto prevCo = co;

        // Function code object
        auto coValue = createCodeObjectValue(
                classObject_ != nullptr ? (classObject_->name + "." + fnName) : fnName,
                arity
                );
        co = AS_CODE(coValue);

        // Put `free` and `cell` vars from the scope into the
        // cellNames of the code object.
        co->freeCount = scopeInfo->free.size();
        co->cellNames.reserve(scopeInfo->free.size() + scopeInfo->cells.size());
        co->cellNames.insert(co->cellNames.end(), scopeInfo->free.begin(), scopeInfo->free.end());
        co->cellNames.insert(co->cellNames.end(), scopeInfo->cells.begin(), scopeInfo->cells.end());

        // Store new co as constant
        prevCo->addConstant(coValue);

        // Function name is registered as local,
        // so the function can call itself recursively
        co->addLocal(fnName);

        // Parameters are added as variables
        for (auto i = 0; i < arity; i++) {
            auto &argName = params[i].string;
            co->addLocal(argName);

            // Note: if the param is captured by the cell, emit the code for it.
            // We also don't pop the param value in this case, since OP_SCOPE_EXIT would pop it.
            auto cellIndex = co->getCellIndex(argName);
            if (cellIndex != -1) {
                emit(OP_SET_CELL);
                emit(cellIndex);
            }
        }
        // Compile body in the new code object
        //
        // Note: reset the current class, so nested blocks
        // and nested closures inside methods are handled.
        auto prevClassObject_ = classObject_;
        classObject_ = nullptr;
        gen(body);
        classObject_ = prevClassObject_;

        if (!isBlock(body)) {
            emit(OP_SCOPE_EXIT);
            emit(arity + 1);
        }

        // Explicit return to restore caller address
        emit(OP_RETURN);

        // == Class methods ==
        if (classObject_ != nullptr) {
            auto fn = ALLOC_FUNCTION(co);
            constantObject_.insert((Traceable*) AS_OBJECT(fn));

            co = prevCo;

            classObject_->properties[fnName] = fn;
        }
        // 1. Simple functions (allocated at compile time).
        // If it's not a closure (doesn't have free variables) allocate it at compile time and store as a constant
        // Closure are allocated at runtime, but reuse the same code object
        else if (scopeInfo->free.size() == 0) {
            // Create the function
            auto fn = ALLOC_FUNCTION(co);
            constantObject_.insert((Traceable *) AS_OBJECT(fn));

            // Restore the code object
            co = prevCo;

            // Add function as a constant to our co
            co->addConstant(fn);

            // And emit code for this new constant
            emit(OP_CONST);
            emit(co->constants.size() - 1);
        }
            // 2. Closures
            // - Load all free vars to capture (indices are taken from the 'cells' of the parent co)
            // - Load code object for the current function
            // - Make function
        else {
            // Restore the code object
            co = prevCo;

            for (const auto &freeVar: scopeInfo->free) {
                emit(OP_LOAD_CELL);
                emit(prevCo->getCellIndex(freeVar));
            }
            emit(OP_CONST);
            emit(co->constants.size() - 1);

            emit(OP_MAKE_FUNCTION);
            emit(scopeInfo->free.size());
        }

        scopeStack_.pop();
    }

    /**
     * Disassemble all compilation units.
     * */
    void disassembleBytecode() {
        for (auto &co_: codeObjects_) {
            disassembler->disassemble(co_);
        }
    }

    /**
     * Return main function (entry point).
     * */
    FunctionObject *getMainFunction() {
        return main;
    }

    /**
     * Returns all constant traceable objects.
     * */
    std::set<Traceable *> &getConstantObjects() {
        return constantObject_;
    }

    /**
     * Currently compiling class object.
     * */
    ClassObject *classObject_;

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
     * Creates a new code object
     * */
    EvaValue createCodeObjectValue(const std::string &name, size_t arity = 0) {
        auto coValue = ALLOC_CODE(name, arity);
        auto co = AS_CODE(coValue);

        codeObjects_.push_back(co);
        constantObject_.insert((Traceable *) co);

        return coValue;
    }

    /**
     * Enters a new block.
     * */
    void blockEnter() {
        co->scopeLevel++;
    }

    /**
     * Exits a block.
     * */
    void blockExit() {
        // Pop vars from the stack if they were declared
        // within this specific scope.
        auto varsCount = getVarsCountOnScopeExit();

        if (varsCount > 0 || co->arity > 0) {
            emit(OP_SCOPE_EXIT);

            if (isFunctionBody()) {
                varsCount += co->arity + 1;
            }
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
     * Whether it's the function body.
     * */
    bool isFunctionBody() {
        return co->name != "main" && co->scopeLevel == 1; // We have implicit block 0, so global scope will be 1
    }

    /**
     * Whether the expression is a decalration.
     * */
    bool isDeclaration(const Exp &exp) { return isVarDeclaration(exp) || isFunctionDeclaration(exp) || isClassDeclaration(exp); }

    /**
     * (var <name> <value>)
     * */
    bool isVarDeclaration(const Exp &exp) {
        return isTaggedList(exp, "var");
    }

    /**
     * (lambda <params> <body>)
     * */
    bool isLambda(const Exp &exp) {
        return isTaggedList(exp, "lambda");
    }

    /**
     * (def <name> ...)
     * */
    bool isFunctionDeclaration(const Exp &exp) {
        return isTaggedList(exp, "def");
    }

    /**
     * (class <name> ...)
     * */
    bool isClassDeclaration(const Exp &exp) {
        return isTaggedList(exp, "class");
    }

    /**
     * (var <name> <value>)
     * */
    bool isBlock(const Exp &exp) {
        return isTaggedList(exp, "begin");
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
        constantObject_.insert((Traceable *) co->constants.back().object);
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

    ClassObject *getClassByName(const std::string &name) {
        for (const auto &classObject: classObjects_) {
            if (classObject->name == name) {
                return classObject;
            }
        }
        return nullptr;
    }

    /**
     * Scope info
     * */
    std::map<const Exp *, std::shared_ptr<Scope>> scopeInfo_;

    /**
     * Scope stack
     * */
    std::stack<std::shared_ptr<Scope>> scopeStack_;

    /**
     * Compiling code object.
     * */
    CodeObject *co;

    /**
     * Main entry point (function).
     * */
    FunctionObject *main;

    /**
     * All code objects.
     * */
    std::vector<CodeObject *> codeObjects_;

    /**
     * All objects from the constant pools of all code objects.
     * */
    std::set<Traceable *> constantObject_;

    /**
     * All class objects.
     * */
    std::vector<ClassObject *> classObjects_;

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

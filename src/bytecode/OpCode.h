#ifndef EVA_VM_OPCODE_H
#define EVA_VM_OPCODE_H

/**
 * Stops the program
 * */
#define OP_HALT 0x00

/**
 * Pushes a const onto the stack.
 * */
#define OP_CONST 0x01

/**
 * Math instruction
 * */
#define OP_ADD 0x02
#define OP_SUB 0x03
#define OP_MUL 0x04
#define OP_DIV 0x05

/**
 * Comparison
 * */
#define OP_COMPARE 0x06

/**
 * Control flow: jump if the value on the stack is false
 * */
#define OP_JMP_IF_FALSE 0x07

/**
 * Unconditional jump.
 * */
#define OP_JMP 0x08

/**
 * Returns global variable.
 * */
#define OP_GET_GLOBAL 0x09

/**
 * Sets a global variable.
 * */
#define OP_SET_GLOBAL 0x10

/**
 * Pops a value from the stack.
 * */
#define OP_POP 0x11

/**
 * Returns a local variable.
 * */
#define OP_GET_LOCAL 0x12

/**
 * Sets a local variable.
 * */
#define OP_SET_LOCAL 0x13

/**
 * Exit scope.
 * */
#define OP_SCOPE_EXIT 0x14

/**
 * Call a function.
 * */
#define OP_CALL 0x15

/**
 * Return from a function
 * */
#define OP_RETURN 0x16

/**
 * Returns a cell.
 * */
#define OP_GET_CELL 0x17

/**
 * Sets a cell.
 * */
#define OP_SET_CELL 0x18

/**
 * Loads cell onto the stack.
 * */
#define OP_LOAD_CELL 0x19

/**
 * Make a function.
 * */
#define OP_MAKE_FUNCTION 0x20

// -------------------------------------------------------

#define OP_STR(op) \
    case OP_##op:      \
      return #op\


std::string opcodeToString(uint8_t opcode) {
    switch (opcode) {
        OP_STR(HALT);
        OP_STR(CONST);
        OP_STR(ADD);
        OP_STR(SUB);
        OP_STR(MUL);
        OP_STR(DIV);
        OP_STR(COMPARE);
        OP_STR(JMP_IF_FALSE);
        OP_STR(JMP);
        OP_STR(GET_GLOBAL);
        OP_STR(SET_GLOBAL);
        OP_STR(POP);
        OP_STR(GET_LOCAL);
        OP_STR(SET_LOCAL);
        OP_STR(SCOPE_EXIT);
        OP_STR(CALL);
        OP_STR(RETURN);
        OP_STR(GET_CELL);
        OP_STR(SET_CELL);
        OP_STR(LOAD_CELL);
        OP_STR(MAKE_FUNCTION);
        default:
            DIE << "opcodeToString: unknown opcode: " << std::hex << (int) opcode;
    };
    return "Unknown"; // Unreachable
}


#endif

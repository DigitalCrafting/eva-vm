#ifndef EVA_VM_EVAVALUE_H
#define EVA_VM_EVAVALUE_H

/**
 * Eva value type.
 * */
enum class EvaValueType {
    NUMBER
};

/**
 * Eva value (tagged union).
 * */
struct EvaValue {
    EvaValueType type;
    union {
        double number;
    };
};

/* ------------------------------------- */
// Constructors:
#define NUMBER(value) ((EvaValue){.type = EvaValueType::NUMBER, .number = (value)})

/* ------------------------------------- */
// Accessor:
#define AS_NUMBER(evaValue) ((double)(evaValue).number)

#endif

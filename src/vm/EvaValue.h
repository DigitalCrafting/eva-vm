#ifndef EVA_VM_EVAVALUE_H
#define EVA_VM_EVAVALUE_H

#include <string>
#include <functional>

/**
 * Eva value type.
 * */
enum class EvaValueType {
    NUMBER,
    BOOLEAN,
    OBJECT
};

/**
 * Object type.
 * */
enum class ObjectType {
    STRING,
    CODE,
    NATIVE
};

/**
 * Base object.
 * */
struct Object {
    explicit Object(ObjectType type) : type(type) {};

    ObjectType type;
};

/**
 * Eva value (tagged union).
 * */
struct EvaValue {
    EvaValueType type;
    union {
        double number;
        bool boolean;
        Object *object;
    };
};

/**
 * String object.
 * */
struct StringObject : public Object {
    explicit StringObject(const std::string &str) : Object(ObjectType::STRING), string(str) {};
    std::string string;
};

/**
 * Native function.
 * */
using NativeFn = std::function<void()>;

struct NativeObject : public Object {
    NativeObject(NativeFn function, const std::string& name, size_t arity)
        : Object(ObjectType::NATIVE),
        function(function),
        name(name),
        arity(arity) {}

    NativeFn function;
    std::string name;
    size_t arity;
};

struct LocalVar {
    std::string name;
    size_t scopeLevel;
};

/**
 * Code object.
 * */
struct CodeObject : public Object {
    CodeObject(const std::string &name) : Object(ObjectType::CODE), name(name) {}

    /**
     * Name of the unit (usually function name).
     * */
    std::string name;

    /**
     * Constant pool.
     * */
    std::vector<EvaValue> constants;

    /**
     * Bytecode.
     * */
    std::vector<uint8_t> code;

    /**
     * Current scope level.
     * */
    size_t scopeLevel = 0;

    /**
     * Local variables and functions.
     * */
    std::vector<LocalVar> locals;

    /**
     *
     * */
     void addLocal(const std::string& name) {
         locals.push_back({name, scopeLevel});
     }

    /**
    * Get local index.
    * */
    int getLocalIndex(const std::string &name) {
        if (!locals.empty()) {
            for (auto i = (int) locals.size() - 1; i >= 0; i--) {
                if (locals[i].name == name) {
                    return i;
                }
            }
        }
        return -1;
    }
};

/* ------------------------------------- */
// Constructors:
#define NUMBER(value) ((EvaValue){.type = EvaValueType::NUMBER, .number = (value)})
#define BOOLEAN(value) ((EvaValue){.type = EvaValueType::BOOLEAN, .boolean = (value)})

#define ALLOC_STRING(value) \
    ((EvaValue){.type = EvaValueType::OBJECT, .object = (Object*)new StringObject(value)})
#define ALLOC_CODE(name) \
    ((EvaValue){.type = EvaValueType::OBJECT, .object = (Object*)new CodeObject(name)})
#define ALLOC_NATIVE(fn, name, arity) \
    ((EvaValue){.type = EvaValueType::OBJECT, .object = (Object*)new NativeObject(fn, name, arity)})

/* ------------------------------------- */
// Accessor:
#define AS_NUMBER(evaValue) ((double)(evaValue).number)
#define AS_BOOLEAN(evaValue) ((bool)(evaValue).boolean)
#define AS_OBJECT(evaValue) ((Object*)(evaValue).object)

#define AS_STRING(evaValue) ((StringObject*)(evaValue).object)
#define AS_CPPSTRING(evaValue) (AS_STRING(evaValue)->string)

#define AS_CODE(evaValue) ((CodeObject*)(evaValue).object)
#define AS_NATIVE(evaValue) ((NativeObject*)(evaValue).object)

/* ------------------------------------- */
// Testers:
#define IS_NUMBER(evaValue) ((evaValue).type == EvaValueType::NUMBER)
#define IS_BOOLEAN(evaValue) ((evaValue).type == EvaValueType::BOOLEAN)
#define IS_OBJECT(evaValue) ((evaValue).type == EvaValueType::OBJECT)

#define IS_OBJECT_TYPE(evaValue, objectType) IS_OBJECT(evaValue) && AS_OBJECT(evaValue)->type == objectType

#define IS_STRING(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::STRING)

#define IS_CODE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::CODE)
#define IS_NATIVE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::NATIVE)

/**
 * String representation used in constants for debug.
 * */
std::string evaValueToTypeString(const EvaValue &evaValue) {
    if (IS_NUMBER(evaValue)) {
        return "NUMBER";
    } else if (IS_BOOLEAN(evaValue)) {
        return "BOOLEAN";
    } else if (IS_STRING(evaValue)) {
        return "STRING";
    } else if (IS_CODE(evaValue)) {
        return "CODE";
    } else if (IS_NATIVE(evaValue)) {
        return "NATIVE";
    } else {
        DIE << "evaValueToTypeString: unknown type " << (int) evaValue.type;
    }
    return ""; // Unreachable
}

/**
 * String representation of constant value used for debug.
 * */
std::string evaValueToConstantString(const EvaValue &evaValue) {
    std::stringstream ss;
    if (IS_NUMBER(evaValue)) {
        ss << evaValue.number;
    } else if (IS_BOOLEAN(evaValue)) {
        ss << (evaValue.boolean ? "true" : "false");
    } else if (IS_STRING(evaValue)) {
        ss << '"' << AS_CPPSTRING(evaValue) << '"';
    } else if (IS_CODE(evaValue)) {
        auto code = AS_CODE(evaValue);
        ss << "code " << code << ": " << code->name;
    } else if (IS_NATIVE(evaValue)) {
        auto fn = AS_NATIVE(evaValue);
        ss << fn->name << "/" << fn->arity;
    } else {
        DIE << "evaValueToConstantString: unknown type " << (int) evaValue.type;
    }
    return ss.str();
}

/**
 * Output stream.
 * */
std::ostream &operator<<(std::ostream &os, const EvaValue &evaValue) {
    return os << "EvaValue (" << evaValueToTypeString(evaValue)
              << "): " << evaValueToConstantString(evaValue) << std::endl;
}

#endif

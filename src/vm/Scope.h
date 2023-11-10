#ifndef EVA_VM_SCOPE_H
#define EVA_VM_SCOPE_H

#include <map>
#include <string>
#include <memory>
#include <set>
#include "Logger.h"

/**
 * Scope type
 * */
enum class ScopeType {
    GLOBAL,
    FUNCTION,
    BLOCK
};

/**
 * Allocation type
 * */
enum class AllocType {
    GLOBAL,
    LOCAL,
    CELL
};

/*
 * Scope structure
 * */
struct Scope {

    Scope(ScopeType type, const std::shared_ptr<Scope> parent)
            : type(type), parent(parent) {}

    /**
     * Register a local.
     * */
    void addLocal(const std::string &name) {
        allocInfo[name] = type == ScopeType::GLOBAL ? AllocType::GLOBAL : AllocType::LOCAL;
    }

    /**
     * Register an own cell.
     * */
    void addCell(const std::string &name) {
        cells.insert(name);
        allocInfo[name] = AllocType::CELL;
    }

    /**
     * Register a free var (parent cell).
     * */
    void addFree(const std::string &name) {
        free.insert(name);
        allocInfo[name] = AllocType::CELL;
    }

    /**
     * Potentially promotes a variable from local to cell
     * */
    void maybePromote(const std::string &name) {
        auto initAllocType =
                type == ScopeType::GLOBAL ? AllocType::GLOBAL : AllocType::LOCAL;

        if (allocInfo.count(name) != 0) {
            initAllocType = allocInfo[name];
        }

        auto [ownerScope, allocType] = resolve(name, initAllocType);

        // Update the alloc type based on resolution
        allocInfo[name] = allocType;

        // If we resolve it as a cell, promote to heap
        if (allocType == AllocType::CELL) {
            promote(name, ownerScope);
        }
    }

    /**
     * Resolve a variable in the scope chain.
     *
     * Initially variable is treated as local, however if during
     * the resolution we passed the own function boundary, it is
     * free, and hence should be promoted to a cell, unless global.
     * */
    std::pair<Scope *, AllocType> resolve(const std::string &name, AllocType allocType) {
        // Found in the current scope
        if (allocInfo.count(name) != 0) {
            return std::make_pair(this, allocType);
        }

        // We crossed the boundary of the function and still didn't
        // resolve a local variable - further resolution should be free
        if (type == ScopeType::FUNCTION) {
            allocType = AllocType::CELL;
        }

        if (parent == nullptr) {
            DIE << "[Scope] Reference error: " << name << " is not defined.";
        }

        // If we resolve in the GLOBAL scope, the resolution is GLOBAL
        if (parent->type == ScopeType::GLOBAL) {
            allocType = AllocType::GLOBAL;
        }

        return parent->resolve(name, allocType);
    }

    /**
     * Returns get opcode based on allocation type.
     * */
     int getNameGetter(const std::string& name) {
        switch (allocInfo[name]) {
            case AllocType::LOCAL:
                return OP_GET_LOCAL;
            case AllocType::CELL:
                return OP_GET_CELL;
            case AllocType::GLOBAL:
                return OP_GET_GLOBAL;
        }
     }

    /**
    * Returns set opcode based on allocation type.
    * */
    int getNameSetter(const std::string& name) {
        switch (allocInfo[name]) {
            case AllocType::LOCAL:
                return OP_SET_LOCAL;
            case AllocType::CELL:
                return OP_SET_CELL;
            case AllocType::GLOBAL:
                return OP_SET_GLOBAL;
        }
    }

    /**
     * Promotes a variable from local (stack) to cell (heap)
     * */
    void promote(const std::string &name, Scope *ownerScope) {
        ownerScope->addCell(name);

        // Treat a variable as free in all parent scopes
        // so it's propagated down to our scope
        auto scope = this;
        while (scope != ownerScope) {
            scope->addFree(name);
            scope = scope->parent.get();
        }
    }


    /**
     * Scope type
     * */
    ScopeType type;

    /**
     * Parent scope
     * */
    std::shared_ptr<Scope> parent;

    /**
     * Allocation info
     * */
    std::map<std::string, AllocType> allocInfo;

    /**
     * Set of free vars
     * */
    std::set<std::string> free;

    /**
     * Set of own cells
     * */
    std::set<std::string> cells;
};

#endif //EVA_VM_SCOPE_H

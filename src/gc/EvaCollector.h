#ifndef EVA_VM_GC_H
#define EVA_VM_GC_H

#include <set>
#include "../vm/EvaValue.h"

struct EvaCollector {
    void gc(std::set<Traceable *> &roots) {
        mark(roots);
        sweep();
    }

    /**
     * Return all pointers withing this object.
     * */
    std::set<Traceable *> getPointers(Traceable *object) {
        std::set<Traceable *> pointers;

        auto evaValue = OBJECT((Object *) object);

        if (IS_FUNCTION(evaValue)) {
            auto fn = AS_FUNCTION(evaValue);
            for (auto &cell: fn->cells) {
                pointers.insert((Traceable *) cell);
            }
        }

        if (IS_INSTANCE(evaValue)) {
            auto instance = AS_INSTANCE(evaValue);
            for (auto &prop: instance->properties) {
                if (IS_OBJECT(prop.second)) {
                    pointers.insert((Traceable *) AS_OBJECT(prop.second));
                }
            }
        }

        return pointers;
    }

    // Marking phase (trace)
    void mark(const std::set<Traceable *> &roots) {
        std::vector<Traceable *> worklist(roots.begin(), roots.end());

        while (!worklist.empty()) {
            auto object = worklist.back();
            worklist.pop_back();

            if (!object->marked) {
                object->marked = true;
                for (auto &p: getPointers(object)) {
                    worklist.push_back(p);
                }
            }
        }
    }

    // Sweep phase (reclaim)
    void sweep() {
        auto it = Traceable::objects.begin();
        while (it != Traceable::objects.end()) {
            auto object = (Traceable *) *it;
            if (object->marked) {
                // Alive object, reset the mark bit for future collection cycles
                object->marked = false;
                ++it;
            } else {
                it = Traceable::objects.erase(it);
                delete object;
            }
        }
    }
};

#endif

#ifndef RIVET_TYPE_H
#define RIVET_TYPE_H

#include <string>

namespace Rivet {
    
    enum class BaseType {
        Void,
        Int,
        String, 
        Unknown
    };

    struct TypeInfo {
        BaseType Base;
        bool IsRef;
        int ArrayCapacity;

        TypeInfo() : Base(BaseType::Unknown), IsRef(false), ArrayCapacity(0) {}
        TypeInfo(BaseType base, bool isRef = false, int arrayCapacity = 0) : Base(base), IsRef(isRef), ArrayCapacity(arrayCapacity) {}

        bool operator==(const TypeInfo& other) const {
            return Base == other.Base && IsRef == other.IsRef && ArrayCapacity == other.ArrayCapacity;
        }

        bool operator!=(const TypeInfo& other) const {
            return !(*this == other);
        }

        bool isArray() const {
            return ArrayCapacity > 0;
        }

        std::string toString() const {
            std::string str;
            switch (Base) {
                case BaseType::Void: str = "void"; break;
                case BaseType::Int:  str = "int"; break;
                case BaseType::String: str = "str"; break;
                default: str = "unknown"; break;
            }
            if (IsRef) str += " ref";
            return str;
        }
    };
}

#endif
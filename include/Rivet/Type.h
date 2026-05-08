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

        TypeInfo() : Base(BaseType::Unknown), IsRef(false) {}
        TypeInfo(BaseType base, bool isRef = false) : Base(base), IsRef(isRef) {}

        bool operator==(const TypeInfo& other) const {
            return Base == other.Base && IsRef == other.IsRef;
        }

        bool operator!=(const TypeInfo& other) const {
            return !(*this == other);
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
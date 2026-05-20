/**
 * @file Type.h
 * @brief Defines the basic types supported by the Rivet type system.
 * 
 * Contains simple enum/struct data layouts intended for use in the ATS evaluation
 * layer and Semantic analysis (e.g., verifying `int` vs `ref int` expressions).
 */
#ifndef RIVET_TYPE_H
#define RIVET_TYPE_H

#include <string>

namespace Rivet {
    
    /**
     * @brief Underlying primitive types natively supported by Rivet.
     */
    enum class BaseType {
        Void,    ///< Represents the absence of type (function returns nothing).
        Int,     ///< 32-bit signed integer type.
        String,  ///< Immutable string literal type.
        Unknown  ///< Unresolved or invalid type.
    };

    /**
     * @struct TypeInfo
     * @brief Encapsulates a data type, its reference semantics, and array capacity if applicable.
     */
    struct TypeInfo {
        BaseType Base;          ///< The foundational primitive type.
        bool IsRef;             ///< Set to true if the type is explicitly requested as a reference.
        int ArrayCapacity;     ///< If > 0, signifies that this type is a fixed-size array block array [N].

        /**
         * @brief Default constructor defaults to a generic Unknown primitive.
         */
        TypeInfo() : Base(BaseType::Unknown), IsRef(false), ArrayCapacity(0) {}
        
        /**
         * @brief Explicitly constructs complete type semantics.
         * @param base The desired primitive layer.
         * @param isRef Whether this is a reference (`ref`) to the type.
         * @param arrayCapacity Default `0` means scalar. Any >0 length becomes a static array.
         */
        TypeInfo(BaseType base, bool isRef = false, int arrayCapacity = 0) : Base(base), IsRef(isRef), ArrayCapacity(arrayCapacity) {}

        /**
         * @brief Checks if two expressions have compatible strict semantic typing.
         * @param other The secondary TypeInfo to compare structural identity with.
         * @return true if primitive types match, array capacities align, and references resolve nicely.
         */
        bool operator==(const TypeInfo& other) const {
            return Base == other.Base && IsRef == other.IsRef && ArrayCapacity == other.ArrayCapacity;
        }

        /**
         * @brief Inverse checking of operator==.
         */
        bool operator!=(const TypeInfo& other) const {
            return !(*this == other);
        }

        /**
         * @brief Utility returning whether this structure resolves as a flat array.
         * @return true if initialized with ArrayCapacity > 0.
         */
        bool isArray() const {
            return ArrayCapacity > 0;
        }

        /**
         * @brief Converts the embedded semantic instructions into a readable debug string format.
         * @return Human-readable string representation like "int", "str ref", etc.
         */
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
/**
 * @file SymbolTable.h
 * @brief Defines the scoped symbol table for semantic analysis.
 * 
 * The SymbolTable resolves variable declarations, scope depths, and lifetime
 * properties during the compilation process, rejecting duplicate declarations
 * and undefined references.
 */
#ifndef RIVET_SYMBOL_TABLE_H
#define RIVET_SYMBOL_TABLE_H

#include "Type.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Rivet {

    /**
     * @struct Symbol
     * @brief Contains data associated with a specific variable inside the symbol table.
     */
    struct Symbol {
        std::string Name;       ///< Identifier name of the variable.
        TypeInfo Type;          ///< Type information for the variable.
        bool IsInitialized;     ///< Tracks whether the variable has been initialized.
        
        // TODO: Add `bool isMutable` to support const/immutable variables.
        // TODO: Add `llvm::Value* MemoryLocation` to unify semantic analysis and code generation,
        //       which would eventually replace `CompilerState.NamedValues`.
    };

    /**
     * @struct FunctionSignature
     * @brief Stores a function's semantic signature independent of LLVM IR.
     *
     * Used by the FunctionRegistry to allow CallAST::typeCheck to validate
     * call sites without touching CompilerState.TheModule at all.
     */
    struct FunctionSignature {
        TypeInfo ReturnType;
        std::vector<TypeInfo> Params;
    };

    /**
     * @class SymbolTable
     * @brief A hierarchical structure for managing variable scopes.
     * 
     * Supports nested scopes (functions, code blocks). When variables are declared, 
     * they are pushed into the current topmost scope. Lookups start at the current 
     * scope and search outwards until the global scope is reached.
     */
    class SymbolTable {
    private:
        std::vector<std::unordered_map<std::string, Symbol>> Scopes; ///< Stack of nested symbol maps.

    public:
        /**
         * @brief Registry of all known function signatures, populated during the
         *        semantic pre-scan pass. CallAST::typeCheck reads from here instead
         *        of from the LLVM module, keeping the two phases fully decoupled.
         */
        std::unordered_map<std::string, FunctionSignature> FunctionRegistry;

        /**
         * @brief Constructs a new SymbolTable and initializes the global scope.
         */
        SymbolTable() {
            enterScope();
        }

        /**
         * @brief Creates a new inner scope and pushes it onto the stack.
         */
        void enterScope() {
            Scopes.push_back(std::unordered_map<std::string, Symbol>());
        }

        /**
         * @brief Destroys the deepest active scope, popping it from the stack.
         */
        void exitScope() {
            if (Scopes.size() > 1) {
                Scopes.pop_back();
            }
        }

        /**
         * @brief Declares a new variable in the latest/current block scope.
         * 
         * @param name The identifier of the variable.
         * @param type The type metadata corresponding to this variable.
         * @param isInitialized Indicates if the variable was defined with an explicit value.
         * @return true if the variable was inserted successfully.
         * @return false if the variable already exists within the *current* restricted scope.
         */
        bool insert(const std::string& name, TypeInfo type, bool isInitialized = false) {
            auto& currentScope = Scopes.back();
            if (currentScope.find(name) != currentScope.end()) {
                return false; // Variable already declared in this exact scope
            }
            currentScope[name] = Symbol{name, type, isInitialized};
            return true;
        }

        /**
         * @brief Searches for a variable traversing upward from the current scope to the global scope.
         * 
         * @param name The identifier to search for.
         * @return Symbol* Pointer to the resolved variable struct, or nullptr if completely undefined.
         */
        Symbol* lookup(const std::string& name) {
            for (auto it = Scopes.rbegin(); it != Scopes.rend(); ++it) {
                if (it->find(name) != it->end()) {
                    return &((*it)[name]);
                }
            }
            return nullptr; 
        }
    };
}

#endif
#ifndef RIVET_SYMBOL_TABLE_H
#define RIVET_SYMBOL_TABLE_H

#include "Type.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Rivet {

    struct Symbol {
        std::string Name;
        TypeInfo Type;
        bool IsInitialized;
    };

    class SymbolTable {
    private:
        std::vector<std::unordered_map<std::string, Symbol>> Scopes;

    public:
        SymbolTable() {
            // Push the global scope
            enterScope();
        }

        void enterScope() {
            Scopes.push_back(std::unordered_map<std::string, Symbol>());
        }

        void exitScope() {
            if (Scopes.size() > 1) {
                Scopes.pop_back();
            }
        }

        // Returns true if successfully inserted, false if variable already exists in CURRENT scope
        bool insert(const std::string& name, TypeInfo type, bool isInitialized = false) {
            auto& currentScope = Scopes.back();
            if (currentScope.find(name) != currentScope.end()) {
                return false; // Variable already declared in this exact scope
            }
            currentScope[name] = Symbol{name, type, isInitialized};
            return true;
        }

        // Looks up a variable starting from the innermost scope outwards
        Symbol* lookup(const std::string& name) {
            for (auto it = Scopes.rbegin(); it != Scopes.rend(); ++it) {
                if (it->find(name) != it->end()) {
                    return &((*it)[name]);
                }
            }
            return nullptr; // Variable not found
        }
    };
}

#endif
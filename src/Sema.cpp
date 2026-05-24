/**
 * @file Sema.cpp
 * @brief Semantic Analysis (Type Checking) for the Rivet compiler.
 *
 * This translation unit contains all typeCheck() and registerSignature()
 * method implementations. It has NO dependency on LLVM IR — it may only
 * include AST.h, SymbolTable.h, and standard library headers.
 *
 * Architectural invariant:
 *   - Do NOT add any #include <llvm/...> to this file.
 *   - All IR-related logic belongs in AST.cpp (codegen) or CodeGen.cpp.
 */

#include "Rivet/AST.h"
#include "Rivet/SymbolTable.h"
#include "Rivet/Lexer.h"
#include <iostream>

namespace Rivet
{
    // -------------------------------------------------------------------------
    // Semantic state — private to this translation unit.
    // These are only ever read/written by typeCheck implementations here.
    // -------------------------------------------------------------------------

    /// Stack of expected return types, one entry per function being checked.
    static std::vector<TypeInfo> FunctionReturnTypeStack;

    /// Set to true by ReturnAST::typeCheck when a return is reached in the
    /// current function body. FunctionAST::typeCheck reads and resets this.
    static bool CurrentFunctionHasReturn = false;

    // =========================================================================
    // Primitive & literal nodes
    // =========================================================================

    bool NumberAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    bool StringLiteralAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        ExprType = TypeInfo(BaseType::String, false);
        return true;
    }

    bool NullLiteralAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        // A null literal is typed as optref (IsRef=true, IsOptRef=true).
        // Base is Int by convention; IsOptRef is the null-safety discriminant.
        ExprType = TypeInfo(BaseType::Int, true, 0, true);
        return true;
    }

    // =========================================================================
    // Variable access
    // =========================================================================

    bool VariableAST::typeCheck(SymbolTable& symTab)
    {
        Symbol* sym = symTab.lookup(Name);
        if (!sym)
        {
            std::cerr << "Semantic Error: Use of undeclared variable '" << Name << "'.\n";
            return false;
        }
        ExprType = sym->Type;
        return true;
    }

    // =========================================================================
    // Variable declaration
    // =========================================================================

    bool VariableDeclAST::typeCheck(SymbolTable& symTab)
    {
        BaseType declaredBase = BaseType::Unknown;
        if (Type == "int")
            declaredBase = BaseType::Int;
        else if (Type == "str")
            declaredBase = BaseType::String;

        if (declaredBase == BaseType::Unknown)
        {
            std::cerr << "Semantic Error: Unknown declared type '" << Type << "' for variable '" << Name << "'.\n";
            return false;
        }

        TypeInfo declaredType(declaredBase, IsRef, ArrayCapacity, IsOptRef);

        if (IsRef && !IsOptRef && !InitVal)
        {
            std::cerr << "Semantic Error: Reference variable '" << Name << "' must be initialized.\n";
            return false;
        }

        if (InitVal)
        {
            if (!InitVal->typeCheck(symTab))
                return false;

            if (InitVal->ExprType != declaredType)
            {
                bool allowed = false;
                // Allow initializing optref with a strict ref of the same base type
                if (declaredType.Base == InitVal->ExprType.Base &&
                    declaredType.IsRef && InitVal->ExprType.IsRef &&
                    declaredType.IsOptRef && !InitVal->ExprType.IsOptRef &&
                    declaredType.ArrayCapacity == InitVal->ExprType.ArrayCapacity)
                {
                    allowed = true;
                }
                // Allow initializing any optref with the null literal
                if (!allowed && declaredType.IsOptRef && InitVal->ExprType.IsOptRef)
                {
                    allowed = true;
                }

                if (!allowed)
                {
                    std::cerr << "Semantic Error: Type mismatch in declaration of '" << Name
                              << "'. Expected " << declaredType.toString()
                              << ", found " << InitVal->ExprType.toString() << ".\n";
                    return false;
                }
            }
        }

        if (!symTab.insert(Name, declaredType, InitVal != nullptr))
        {
            std::cerr << "Semantic Error: Variable '" << Name << "' already declared in this scope.\n";
            return false;
        }

        ExprType = declaredType;
        return true;
    }

    // =========================================================================
    // Expressions
    // =========================================================================

    bool BinaryOpAST::typeCheck(SymbolTable& symTab)
    {
        if (!LHS->typeCheck(symTab) || !RHS->typeCheck(symTab))
            return false;

        if (Op == '=')
        {
            bool isVariable = dynamic_cast<VariableAST*>(LHS.get()) != nullptr;
            bool isDeref    = dynamic_cast<DerefAST*>(LHS.get())    != nullptr;
            bool isIndex    = dynamic_cast<IndexAST*>(LHS.get())     != nullptr;

            if (!isVariable && !isDeref && !isIndex)
            {
                std::cerr << "Semantic Error: Left Hand Side of assignment must be a variable, dereference, or array index.\n";
                return false;
            }
            if (LHS->ExprType != RHS->ExprType)
            {
                bool allowed = false;
                // Allow assigning a strict ref to an optref of the same base
                if (LHS->ExprType.Base == RHS->ExprType.Base &&
                    LHS->ExprType.IsRef && RHS->ExprType.IsRef &&
                    LHS->ExprType.IsOptRef && !RHS->ExprType.IsOptRef &&
                    LHS->ExprType.ArrayCapacity == RHS->ExprType.ArrayCapacity)
                {
                    allowed = true;
                }
                // Allow assigning the null literal to any optref
                if (!allowed && LHS->ExprType.IsOptRef && RHS->ExprType.IsOptRef)
                {
                    allowed = true;
                }

                if (!allowed)
                {
                    std::cerr << "Semantic Error: Types of LHS and RHS must be the same.\n";
                    return false;
                }
            }
            ExprType = LHS->ExprType;
            return true;
        }

        if (LHS->ExprType.Base != BaseType::Int || RHS->ExprType.Base != BaseType::Int ||
            LHS->ExprType.IsRef || RHS->ExprType.IsRef ||
            LHS->ExprType.isArray() || RHS->ExprType.isArray())
        {
            std::cerr << "Semantic Error: Binary operators require integer operands.\n";
            return false;
        }

        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    bool UnaryOpAST::typeCheck(SymbolTable& symTab)
    {
        if (!Operand->typeCheck(symTab))
            return false;

        // Current unary operators are defined only for plain integer values.
        if (Operand->ExprType.Base != BaseType::Int || Operand->ExprType.IsRef || Operand->ExprType.isArray())
        {
            std::cerr << "Semantic Error: Unary operator requires a non-reference int operand.\n";
            return false;
        }

        switch (Op)
        {
            case '-':
            case tok_not:
                ExprType = TypeInfo(BaseType::Int, false);
                return true;
            default:
                std::cerr << "Semantic Error: Unknown unary operator in type check: " << Op << ".\n";
                return false;
        }
    }

    // =========================================================================
    // Control flow
    // =========================================================================

    bool BlockAST::typeCheck(SymbolTable& symTab)
    {
        symTab.enterScope();

        // Save the caller's has-return state; bubble ours up afterwards.
        bool savedHasReturn = CurrentFunctionHasReturn;
        CurrentFunctionHasReturn = false;

        bool hasReturnInBlock = false;
        for (auto& stmt : Statements)
        {
            if (!stmt->typeCheck(symTab))
            {
                symTab.exitScope();
                CurrentFunctionHasReturn = savedHasReturn || hasReturnInBlock;
                return false;
            }
            if (CurrentFunctionHasReturn)
                hasReturnInBlock = true;
        }

        symTab.exitScope();
        // Propagate: outer scope sees a return if this block or any inner block had one.
        CurrentFunctionHasReturn = savedHasReturn || hasReturnInBlock;
        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    bool IfAST::typeCheck(SymbolTable& symTab)
    {
        if (!Cond->typeCheck(symTab))  return false;
        if (!Then->typeCheck(symTab))  return false;
        if (Else && !Else->typeCheck(symTab)) return false;

        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    bool WhileAST::typeCheck(SymbolTable& symTab)
    {
        if (!Cond->typeCheck(symTab)) return false;
        if (!Body->typeCheck(symTab)) return false;

        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    bool ForAST::typeCheck(SymbolTable& symTab)
    {
        if (Kind == LoopKind::Array)
        {
            Symbol *ArraySym = symTab.lookup(ArrayName);
            if (!ArraySym)
            {
                std::cerr << "Semantic Error: Array '" << ArrayName << "' is not declared.\n";
                return false;
            }
            if (!ArraySym->Type.isArray())
            {
                std::cerr << "Semantic Error: Variable '" << ArrayName << "' is not an array.\n";
                return false;
            }

            symTab.enterScope();
            symTab.insert(VarName, TypeInfo(ArraySym->Type.Base, false), true);
            bool bodyValid = Body->typeCheck(symTab);
            symTab.exitScope();

            ExprType = TypeInfo(BaseType::Int, false);
            return bodyValid;
        }

        if (!Start->typeCheck(symTab) || !End->typeCheck(symTab))
            return false;
        if (Step && !Step->typeCheck(symTab))
            return false;

        symTab.enterScope();
        symTab.insert(VarName, TypeInfo(BaseType::Int, false), true);

        bool bodyValid = Body->typeCheck(symTab);

        symTab.exitScope();
        ExprType = TypeInfo(BaseType::Int, false);
        return bodyValid;
    }

    // =========================================================================
    // Function calls
    // =========================================================================

    bool CallAST::typeCheck(SymbolTable& symTab)
    {
        // Look up purely from the semantic FunctionRegistry — no LLVM module access.
        auto it = symTab.FunctionRegistry.find(Callee);
        if (it == symTab.FunctionRegistry.end())
        {
            std::cerr << "Semantic Error: Unknown function '" << Callee << "'.\n";
            return false;
        }

        const FunctionSignature& Sig = it->second;

        if (Sig.Params.size() != Args.size())
        {
            std::cerr << "Semantic Error: Incorrect number of arguments passed to function '"
                      << Callee << "'. Expected " << Sig.Params.size()
                      << ", got " << Args.size() << ".\n";
            return false;
        }

        for (size_t i = 0; i < Args.size(); ++i)
        {
            if (!Args[i]->typeCheck(symTab))
                return false;

            const TypeInfo& Expected = Sig.Params[i];
            const TypeInfo& Actual   = Args[i]->ExprType;

            bool compatible = (Expected == Actual);
            // Allow passing a strict ref where an optref is expected
            if (!compatible && Expected.Base == Actual.Base &&
                Expected.IsRef && Actual.IsRef &&
                Expected.IsOptRef && !Actual.IsOptRef)
                compatible = true;

            if (!compatible)
            {
                std::cerr << "Semantic Error: Type mismatch for argument " << i
                          << " in call to '" << Callee << "'. Expected "
                          << Expected.toString() << ", found " << Actual.toString() << ".\n";
                return false;
            }
        }

        ExprType = Sig.ReturnType;
        return true;
    }

    // =========================================================================
    // Return statement
    // =========================================================================

    bool ReturnAST::typeCheck(SymbolTable& symTab)
    {
        if (FunctionReturnTypeStack.empty())
        {
            std::cerr << "Semantic Error: 'return' used outside of a function.\n";
            return false;
        }

        TypeInfo expected = FunctionReturnTypeStack.back();
        if (!RetVal)
        {
            if (expected.Base != BaseType::Void)
            {
                std::cerr << "Semantic Error: Non-void function must return a value.\n";
                return false;
            }
            ExprType = TypeInfo(BaseType::Void, false);
            return true;
        }

        if (!RetVal->typeCheck(symTab))
            return false;

        if (expected.Base == BaseType::Void)
        {
            std::cerr << "Semantic Error: Void function cannot return a value.\n";
            return false;
        }
        if (RetVal->ExprType != expected)
        {
            std::cerr << "Semantic Error: Return type mismatch. Expected "
                      << expected.toString() << ", found " << RetVal->ExprType.toString() << ".\n";
            return false;
        }
        ExprType = expected;
        // Signal to FunctionAST that this code path reaches a return.
        CurrentFunctionHasReturn = true;
        return true;
    }

    // =========================================================================
    // Function declaration — two steps used by main.cpp
    // =========================================================================

    bool FunctionAST::registerSignature(SymbolTable& symTab)
    {
        auto toTypeInfo = [&](const std::string& typeName, bool isRef = false, bool isOptRef = false) -> TypeInfo
        {
            if (typeName == "int")   return TypeInfo(BaseType::Int,    isRef, 0, isOptRef);
            if (typeName == "str")   return TypeInfo(BaseType::String, isRef, 0, isOptRef);
            if (typeName == "void")  return TypeInfo(BaseType::Void,   false);
            return TypeInfo(BaseType::Unknown, false);
        };

        TypeInfo RetInfo = toTypeInfo(ReturnType, false);
        if (RetInfo.Base == BaseType::Unknown)
        {
            std::cerr << "Semantic Error: Unknown return type '" << ReturnType << "' in function '" << Name << "'.\n";
            return false;
        }

        std::vector<TypeInfo> ParamTypes;
        ParamTypes.reserve(Params.size());
        for (const auto& P : Params)
        {
            TypeInfo TI = toTypeInfo(P.TypeName, P.IsRef, P.IsOptRef);
            if (TI.Base == BaseType::Unknown)
            {
                std::cerr << "Semantic Error: Unknown parameter type '" << P.TypeName << "' in function '" << Name << "'.\n";
                return false;
            }
            if (TI.Base == BaseType::Void)
            {
                std::cerr << "Semantic Error: Function parameters cannot be void in function '" << Name << "'.\n";
                return false;
            }
            ParamTypes.push_back(TI);
        }

        symTab.FunctionRegistry[Name] = FunctionSignature{RetInfo, ParamTypes};
        return true;
    }

    bool FunctionAST::typeCheck(SymbolTable& symTab)
    {
        auto toTypeInfo = [&](const std::string& typeName, bool isRef = false, bool isOptRef = false) -> TypeInfo
        {
            if (typeName == "int")
                return TypeInfo(BaseType::Int, isRef, 0, isOptRef);
            if (typeName == "str")
                return TypeInfo(BaseType::String, isRef, 0, isOptRef);
            if (typeName == "void")
                return TypeInfo(BaseType::Void, false);
            return TypeInfo(BaseType::Unknown, false);
        };

        TypeInfo RetInfo = toTypeInfo(ReturnType, false);
        if (RetInfo.Base == BaseType::Unknown)
        {
            std::cerr << "Semantic Error: Unknown return type '" << ReturnType << "' in function '" << Name << "'.\n";
            return false;
        }

        // Build the parameter TypeInfo list and validate each one.
        std::vector<TypeInfo> ParamTypes;
        ParamTypes.reserve(Params.size());
        for (const auto& P : Params)
        {
            TypeInfo TI = toTypeInfo(P.TypeName, P.IsRef, P.IsOptRef);
            if (TI.Base == BaseType::Unknown)
            {
                std::cerr << "Semantic Error: Unknown parameter type '" << P.TypeName << "' in function '" << Name << "'.\n";
                return false;
            }
            if (TI.Base == BaseType::Void)
            {
                std::cerr << "Semantic Error: Function parameters cannot be void in function '" << Name << "'.\n";
                return false;
            }
            ParamTypes.push_back(TI);
        }

        // Register signature into FunctionRegistry — NO LLVM IR created here.
        symTab.FunctionRegistry[Name] = FunctionSignature{RetInfo, ParamTypes};

        symTab.enterScope();
        for (size_t i = 0; i < Params.size(); ++i)
        {
            if (!symTab.insert(Params[i].Name, ParamTypes[i], true))
            {
                std::cerr << "Semantic Error: Duplicate parameter name '" << Params[i].Name << "' in function '" << Name << "'.\n";
                symTab.exitScope();
                return false;
            }
        }

        FunctionReturnTypeStack.push_back(RetInfo);
        // Reset the flag before checking the body of this function.
        CurrentFunctionHasReturn = false;
        bool BodyOk = Body->typeCheck(symTab);
        bool bodyHasReturn = CurrentFunctionHasReturn;
        FunctionReturnTypeStack.pop_back();
        symTab.exitScope();

        if (BodyOk && RetInfo.Base != BaseType::Void && !bodyHasReturn)
        {
            std::cerr << "Semantic Error: Non-void function '" << Name
                      << "' does not always return a value.\n";
            return false;
        }

        ExprType = TypeInfo(BaseType::Void, false);
        return BodyOk;
    }

    // =========================================================================
    // Import / module nodes
    // =========================================================================

    bool ImportAST::typeCheck(SymbolTable& symTab)
    {
        bool success = true;
        for (const auto& Node : ImportedNodes)
        {
            if (!Node->typeCheck(symTab))
                success = false;
        }
        return success;
    }

    // =========================================================================
    // Pointer / reference nodes
    // =========================================================================

    bool AddressOfAST::typeCheck(SymbolTable& symTab)
    {
        Symbol* sym = symTab.lookup(VarName);
        if (!sym)
        {
            std::cerr << "Semantic Error: Cannot take address of undeclared variable '" << VarName << "'.\n";
            return false;
        }
        ExprType = TypeInfo(sym->Type.Base, true);
        return true;
    }

    bool DerefAST::typeCheck(SymbolTable& symTab)
    {
        if (!Operand->typeCheck(symTab))
            return false;

        if (!Operand->ExprType.IsRef)
        {
            std::cerr << "Semantic Error: Cannot dereference a non-reference type ("
                      << Operand->ExprType.toString() << ").\n";
            return false;
        }

        ExprType = TypeInfo(Operand->ExprType.Base, false);
        return true;
    }

    // =========================================================================
    // Array indexing
    // =========================================================================

    bool IndexAST::typeCheck(SymbolTable& symTab)
    {
        // Verify the variable exists in symbol table
        Symbol* sym = symTab.lookup(ArrayName);
        if (!sym)
        {
            std::cerr << "Semantic Error: Array '" << ArrayName << "' is not declared.\n";
            return false;
        }

        // Verify it's an array
        if (!sym->Type.isArray())
        {
            std::cerr << "Semantic Error: Variable '" << ArrayName << "' is not an array.\n";
            return false;
        }

        // Verify the index expression is valid and resolves to an integer
        if (!IndexExpr->typeCheck(symTab))
            return false;
        if (IndexExpr->ExprType.Base != BaseType::Int || IndexExpr->ExprType.IsRef || IndexExpr->ExprType.isArray())
        {
            std::cerr << "Semantic Error: Array '" << ArrayName << "' must be of type int.\n";
            return false;
        }

        // Compile-time bounds check: if the index is a hardcoded literal
        if (auto* NumNode = dynamic_cast<NumberAST*>(IndexExpr.get()))
        {
            int indexValue = NumNode->getVal();
            if (indexValue < 0 || indexValue >= sym->Type.ArrayCapacity)
            {
                std::cerr << "Semantic Error: Array index out of bounds. '" << ArrayName
                          << "' has capacity " << sym->Type.ArrayCapacity
                          << ", but accessed at index " << indexValue << ".\n";
                return false;
            }
        }

        // Indexing int[N] yields int
        ExprType = TypeInfo(sym->Type.Base, false);
        return true;
    }

} // namespace Rivet

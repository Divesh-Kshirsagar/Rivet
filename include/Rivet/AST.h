/**
 * @file AST.h
 * @brief Defines the Abstract Syntax Tree classes for the Rivet compiler.
 * 
 * Provides node structures that represent structural elements of code 
 * (Expressions, Statements, Variables, Control Flow). Every node supports 
 * type checking (`typeCheck`) and LLVM IR generation (`codegen`).
 */
#ifndef RIVET_AST_H
#define RIVET_AST_H

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "Type.h"
#include "SymbolTable.h"

namespace llvm
{
    class Value;
    class Function;
}

namespace Rivet
{
    struct FunctionParam
    {
        std::string TypeName;
        std::string Name;
        bool IsRef;
        bool IsOptRef;
    };

    /**
     * @class ASTNode
     * @brief The pure virtual base class for all Abstract Syntax Tree nodes.
     * 
     * Every discrete logical step in a Rivet program inherits from this class.
     * It ensures all constructs implement code generation, type checking, and AST tree visualizations.
     */
    class ASTNode
    {
    public:
        virtual ~ASTNode() = default;

        /**
         * @brief Transforms the node's logical definition into corresponding LLVM instructions.
         * @return llvm::Value* A pointer to an emitted LLVM value, or nullptr on failure.
         */
        virtual llvm::Value *codegen() = 0;

        /**
         * @brief Recursively prints out the structure of the AST via stdout.
         * @param indent Defines how many spaces to apply to format tree depth accurately.
         */
        virtual void dump(int indent = 0) const = 0;

        TypeInfo ExprType; ///< Discovered or annotated resulting type of this specific node expression.
        
        /**
         * @brief Analyzes AST structure semantics like variable lifetimes, references, bounds, and typing.
         * @param symTab Mutable reference to an ongoing scope tracker mapping variable names.
         * @return true if semantically valid and verified. False if illegal structures (memory constraints, missing vars) exist.
         */
        virtual bool typeCheck(SymbolTable& symTab) = 0; 

    protected:
        /**
         * @brief A helper to simplify recursively nested string output in Dumps.
         */
        void printIndent(int indent) const
        {
            for (int i = 0; i < indent; ++i)
                std::cout << "  ";
        }
    };

    /**
     * @class NumberAST
     * @brief Syntactic node covering integer literal definitions.
     */
    class NumberAST : public ASTNode
    {
        int Val; ///< The parsed integer value.

    public:
        NumberAST(int Val) : Val(Val) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        int getVal() const { return Val; }
    };

    /**
     * @class StringLiteralAST
     * @brief Node containing immutable string literals (`"sample"`).
     */
    class StringLiteralAST : public ASTNode
    {
        std::string Val; ///< The complete string text.

    public:
        StringLiteralAST(const std::string &Val) : Val(Val) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        const std::string &getVal() const { return Val; }
    };

    /**
     * @class NullLiteralAST
     * @brief Represents the `null` keyword — a typed null pointer literal.
     * 
     * Only valid as a value for `optref` typed variables. Emits a null pointer
     * constant in LLVM IR and carries an optref TypeInfo so the type checker
     * can validate it is only assigned to optional reference targets.
     */
    class NullLiteralAST : public ASTNode
    {
    public:
        NullLiteralAST() {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };
    
    /**
     * @class VariableAST
     * @brief Refers to an already existing defined symbol/variable by its name string.
     */
    class VariableAST : public ASTNode
    {
        std::string Name; ///< The exact name of the requested symbol.

    public:
        VariableAST(const std::string &Name) : Name(Name) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        const std::string &getName() const { return Name; }
    };

    /**
     * @class VariableDeclAST
     * @brief Controls variable allocation syntax like `int x = 5;` or `int[5] array;`.
     */
    class VariableDeclAST : public ASTNode
    {
        std::string Type; ///< String identifier of the memory type (e.g. "int", "str").
        std::string Name; ///< Variable name.
        bool IsRef;       ///< True if constructed as a reference type.
        bool IsOptRef;
        std::unique_ptr<ASTNode> InitVal; ///< The assignment expression (can be null if uninitialized).
        int ArrayCapacity; ///< Set to >0 if defining a fixed-length static array block.
    public:
        VariableDeclAST(const std::string &Type, const std::string &Name, bool IsRef, bool IsOptRef, std::unique_ptr<ASTNode> InitVal, int ArrayCapacity)
            : Type(Type), Name(Name), IsRef(IsRef), IsOptRef(IsOptRef), InitVal(std::move(InitVal)), ArrayCapacity(ArrayCapacity) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class BinaryOpAST
     * @brief Node containing binary operators (e.g. +, -, *, /) and LHS/RHS operands.
     */
    class BinaryOpAST : public ASTNode
    {
        int Op; ///< The matching Lexer token ID or char of the operator.
        std::unique_ptr<ASTNode> LHS; ///< Left-hand side operand.
        std::unique_ptr<ASTNode> RHS; ///< Right-hand side operand.

    public:
        BinaryOpAST(int Op, std::unique_ptr<ASTNode> LHS, std::unique_ptr<ASTNode> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class UnaryOpAST
     * @brief Node describing unary operators applied to a single operand (e.g. -x, !x).
     */
    class UnaryOpAST : public ASTNode
    {
        int Op; ///< The operator token (e.g. '-', tok_not).
        std::unique_ptr<ASTNode> Operand; ///< Target nested expression.

    public:
        UnaryOpAST(int Op, std::unique_ptr<ASTNode> Operand)
            : Op(Op), Operand(std::move(Operand)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class BlockAST
     * @brief Serves as a container for sequential statements grouping variable scopes `{ ... }`.
     */
    class BlockAST : public ASTNode
    {
        std::vector<std::unique_ptr<ASTNode>> Statements; ///< Array of sub-nodes executing linearly.

    public:
        BlockAST(std::vector<std::unique_ptr<ASTNode>> Statements)
            : Statements(std::move(Statements)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class IfAST
     * @brief Control flow conditional branching (if-else expressions).
     */
    class IfAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Cond; ///< Verification expression.
        std::unique_ptr<ASTNode> Then; ///< Block executed on truth.
        std::unique_ptr<ASTNode> Else; ///< Optional block executed on falsity (can be null).

    public:
        IfAST(std::unique_ptr<ASTNode> Cond, std::unique_ptr<ASTNode> Then, std::unique_ptr<ASTNode> Else)
            : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class WhileAST
     * @brief A while loop expression iterating execution while Cond evaluates to non-zero.
     */
    class WhileAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Cond; ///< Condition re-evaluated per loop.
        std::unique_ptr<ASTNode> Body; ///< Interior logic Block.

    public:
        WhileAST(std::unique_ptr<ASTNode> Cond, std::unique_ptr<ASTNode> Body)
            : Cond(std::move(Cond)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class ForAST
     * @brief A for-loop structure defining numeric iterator stepping.
     */
    class ForAST : public ASTNode
    {
    public:
        enum class LoopKind
        {
            Range,
            Array
        };

    private:
        std::string VarName; ///< Scope-local iterator variable.
        LoopKind Kind; ///< Distinguishes range loops from array iteration loops.
        std::unique_ptr<ASTNode> Start; ///< The initial loop value assignment.
        std::unique_ptr<ASTNode> End; ///< The boundary target.
        std::unique_ptr<ASTNode> Step; ///< Re-increment interval after each loop.
        std::string ArrayName; ///< Source array for array iteration loops.
        std::unique_ptr<ASTNode> Body; ///< Logic execution block.

    public:
        ForAST(const std::string &VarName, std::unique_ptr<ASTNode> Start, std::unique_ptr<ASTNode> End, std::unique_ptr<ASTNode> Step, std::unique_ptr<ASTNode> Body)
            : VarName(VarName), Kind(LoopKind::Range), Start(std::move(Start)), End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}
        ForAST(const std::string &VarName, const std::string &ArrayName, std::unique_ptr<ASTNode> Body)
            : VarName(VarName), Kind(LoopKind::Array), ArrayName(ArrayName), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class CallAST
     * @brief Encapsulates function or routine invocation mapping to its identifier and parameters.
     */
    class CallAST : public ASTNode
    {
        std::string Callee; ///< Specified name of the function to invoke.
        std::vector<std::unique_ptr<ASTNode>> Args; ///< Evaluated arguments bound to the Call instruction.

    public:
        CallAST(const std::string &Callee, std::vector<std::unique_ptr<ASTNode>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class ReturnAST
     * @brief Represents a `return` statement inside a function body.
     */
    class ReturnAST : public ASTNode
    {
        std::unique_ptr<ASTNode> RetVal;

    public:
        ReturnAST(std::unique_ptr<ASTNode> RetVal)
            : RetVal(std::move(RetVal)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class FunctionAST
     * @brief Represents a function declaration/definition.
     */
    class FunctionAST : public ASTNode
    {
        std::string Name;
        std::string ReturnType;
        std::vector<FunctionParam> Params;
        std::unique_ptr<ASTNode> Body;

    public:
        FunctionAST(const std::string &Name, const std::string &ReturnType, std::vector<FunctionParam> Params, std::unique_ptr<ASTNode> Body)
            : Name(Name), ReturnType(ReturnType), Params(std::move(Params)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;

        /**
         * @brief Pre-scan step: registers this function's signature (return type + param types)
         *        into symTab.FunctionRegistry without type-checking the body.
         *        Called in a first pass so that all function signatures are known before
         *        any body is type-checked, enabling forward declarations and mutual recursion.
         * @return true on success, false if signature has unknown types.
         */
        bool registerSignature(SymbolTable& symTab);

        /**
         * @brief Codegen pre-pass: creates the LLVM function declaration (prototype)
         *        without emitting a body. Called before any function body is codegen'd
         *        so CallAST::codegen can always find its callee via getFunction().
         */
        llvm::Function *createPrototype();
    };

    /**
     * @class ImportAST
     * @brief Pulls external files/modules into the current code compilation tree.
     */
    class ImportAST : public ASTNode
    {
        std::string ModuleName; ///< Resolvable path or internal name.
        std::vector<std::unique_ptr<ASTNode>> ImportedNodes; ///< Collection of root AST nodes sourced externally.

    public:
        ImportAST(const std::string &ModuleName, std::vector<std::unique_ptr<ASTNode>> ImportedNodes)
            : ModuleName(ModuleName), ImportedNodes(std::move(ImportedNodes)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class AddressOfAST
     * @brief Equivalent to the C `&` operator to retrieve pointer locations.
     */
    class AddressOfAST : public ASTNode
    {
        std::string VarName; ///< Name of the target variable to query its address in memory.

    public:
        AddressOfAST(const std::string &VarName)
            : VarName(VarName) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /**
     * @class DerefAST
     * @brief Equivalent to the C `*` operator pointing into referenced memory.
     */
    class DerefAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Operand; ///< AST resolving to the target pointer data.

    public:
        DerefAST(std::unique_ptr<ASTNode> Operand)
            : Operand(std::move(Operand)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        // Getter for the assignment of the pointer expression
        ASTNode *getOperand() const { return Operand.get(); }
    };

    /**
     * @class IndexAST
     * @brief Node containing array indexing statements (`x[i]`).
     */
    class IndexAST : public ASTNode
    {
        std::string ArrayName; ///< Identifier mapped to the target array.
        std::unique_ptr<ASTNode> IndexExpr; ///< Resolvable subscript index (`i` in `x[i]`).

    public:
        IndexAST(const std::string &ArrayName, std::unique_ptr<ASTNode> IndexExpr)
            : ArrayName(ArrayName), IndexExpr(std::move(IndexExpr)) {}
        llvm::Value *codegenAddress();
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        std::string getArrayName() const { return ArrayName; }
        ASTNode *getIndexExpr() const { return IndexExpr.get(); }
    };

    /**
     * @class VolatileStoreAST
     * @brief Compiler intrinsic for hardware-mapped I/O writes.
     *
     * Maps `__volatile_store(address, value)` to LLVM's `inttoptr` + volatile `store`.
     * This prevents the optimizer from eliminating writes to memory-mapped
     * peripheral registers — essential for embedded I/O (UART, GPIO, etc.).
     *
     * @todo Addresses above 0x7FFFFFFF will overflow signed int — add `uint` type
     *       to support full 32-bit address space (e.g. NVIC at 0xE000E000).
     */
    class VolatileStoreAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Address;
        std::unique_ptr<ASTNode> Value;

    public:
        VolatileStoreAST(std::unique_ptr<ASTNode> address, std::unique_ptr<ASTNode> value)
            : Address(std::move(address)), Value(std::move(value)) {}

        llvm::Value *codegen() override;
        bool typeCheck(SymbolTable &symTab) override;
        void dump(int indent = 0) const override;
    };
}

#endif

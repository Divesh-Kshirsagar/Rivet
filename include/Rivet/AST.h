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
    // TODO: Implement codegen
    class ASTNode
    {
    public:
        virtual ~ASTNode() = default;
        virtual llvm::Value *codegen() = 0;
        virtual void dump(int indent = 0) const = 0;

        TypeInfo ExprType;
        virtual bool typeCheck(SymbolTable& symTab) = 0; // Returns true if the node is semantically valid, false if an error occurred.


    protected:
        void printIndent(int indent) const
        {
            for (int i = 0; i < indent; ++i)
                std::cout << "  ";
        }
    };

    class NumberAST : public ASTNode
    {
        int Val;

    public:
        NumberAST(int Val) : Val(Val) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class VariableAST : public ASTNode
    {
        std::string Name;

    public:
        VariableAST(const std::string &Name) : Name(Name) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        const std::string &getName() const { return Name; }
    };

    class VariableDeclAST : public ASTNode
    {
        std::string Type;
        std::string Name;
        bool IsRef; // tracks if the variable is a pointer
        std::unique_ptr<ASTNode> InitVal; // Can be null if uninitialized
    public:
        VariableDeclAST(const std::string &Type, const std::string &Name, bool IsRef, std::unique_ptr<ASTNode> InitVal)
            : Type(Type), Name(Name), IsRef(IsRef), InitVal(std::move(InitVal)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class BinaryOpAST : public ASTNode
    {
        int Op;
        std::unique_ptr<ASTNode> LHS, RHS;

    public:
        BinaryOpAST(int Op, std::unique_ptr<ASTNode> LHS, std::unique_ptr<ASTNode> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class UnaryOpAST : public ASTNode
    {
        int Op;
        std::unique_ptr<ASTNode> Operand;

    public:
        UnaryOpAST(int Op, std::unique_ptr<ASTNode> Operand)
            : Op(Op), Operand(std::move(Operand)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class BlockAST : public ASTNode
    {
        std::vector<std::unique_ptr<ASTNode>> Statements;

    public:
        BlockAST(std::vector<std::unique_ptr<ASTNode>> Statements)
            : Statements(std::move(Statements)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class IfAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Cond, Then, Else;

    public:
        IfAST(std::unique_ptr<ASTNode> Cond, std::unique_ptr<ASTNode> Then, std::unique_ptr<ASTNode> Else)
            : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class WhileAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Cond, Body;

    public:
        WhileAST(std::unique_ptr<ASTNode> Cond, std::unique_ptr<ASTNode> Body)
            : Cond(std::move(Cond)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    /*
     * For loop in Rivet Language
     * 
     */
    class ForAST : public ASTNode
    {
        std::string VarName;
        std::unique_ptr<ASTNode> Start, End, Step, Body;

        public:
        ForAST(const std::string &VarName, std::unique_ptr<ASTNode> Start, std::unique_ptr<ASTNode> End, std::unique_ptr<ASTNode> Step, std::unique_ptr<ASTNode> Body)
            : VarName(VarName), Start(std::move(Start)), End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class CallAST : public ASTNode
    {
        std::string Callee;
        std::vector<std::unique_ptr<ASTNode>> Args;

    public:
        CallAST(const std::string &Callee, std::vector<std::unique_ptr<ASTNode>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class ImportAST : public ASTNode
    {
        std::string ModuleName;
        std::vector<std::unique_ptr<ASTNode>> ImportedNodes;

    public:
        ImportAST(const std::string &ModuleName, std::vector<std::unique_ptr<ASTNode>> ImportedNodes)
            : ModuleName(ModuleName), ImportedNodes(std::move(ImportedNodes)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class AddressOfAST : public ASTNode
    {
        std::string VarName;

    public:
        AddressOfAST(const std::string &VarName)
            : VarName(VarName) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
    };

    class DerefAST : public ASTNode
    {
        std::unique_ptr<ASTNode> Operand;

    public:
        DerefAST(std::unique_ptr<ASTNode> Operand)
            : Operand(std::move(Operand)) {}
        llvm::Value *codegen() override;
        void dump(int indent = 0) const override;
        bool typeCheck(SymbolTable& symTab) override;
        // Getter for the assignment of the pointer expression
        ASTNode *getOperand() const { return Operand.get(); }
    };
}

#endif

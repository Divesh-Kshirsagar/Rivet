/**
 * @file AST.cpp
 * @brief Implementation for AST node type-checking and log dumps.
 * 
 * Defines how expressions behave dynamically when traversing code tree structures.
 * Also configures exactly what information is displayed via `--dump-ast`.
 */
#include "Rivet/AST.h"
#include "Rivet/Lexer.h"
#include "Rivet/CodeGen.h"
#include <iostream>
#include <vector>
#include <llvm/IR/Constant.h>

namespace Rivet
{
    static std::vector<TypeInfo> FunctionReturnTypeStack;

    void NumberAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Number: " << Val << std::endl;
    }
    llvm::Value *NumberAST::codegen()
    {
        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, Val, true));
    }
    bool NumberAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    void VariableAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Variable: " << Name << std::endl;
    }
    llvm::Value *VariableAST::codegen()
    {
        llvm::AllocaInst *Alloca = CompilerState.NamedValues[Name];
        if (!Alloca)
        {
            std::cerr << "Unknown variable name: " << Name << std::endl;
            return nullptr;
        }
        return CompilerState.Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, Name.c_str());
    }
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

    void VariableDeclAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Variable Declaration: " << Type << " " << Name;
        if (InitVal)
        {
            std::cout << " = ";
            InitVal->dump(0);
        }
        else
        {
            std::cout << " (uninitialized)" << std::endl;
        }
    }
    llvm::Value *VariableDeclAST::codegen()
    {
        llvm::Value *InitValIR = nullptr;       
        llvm::Type *VarType = nullptr;

        if (ExprType.isArray())
        {
            llvm::Type *ElemType = llvm::IntegerType::getInt32Ty(*CompilerState.TheContext);
            VarType = llvm::ArrayType::get(ElemType, ExprType.ArrayCapacity);
        }
        else if (ExprType.Base == BaseType::String)
        {
            VarType = CompilerState.StringStructType;
        }
        else if (IsRef)
        {    
            // pointer
            VarType = llvm::PointerType::getUnqual(*CompilerState.TheContext);
        }
        else
        {
            // i32
            VarType = llvm::IntegerType::getInt32Ty(*CompilerState.TheContext);
        }

        llvm::AllocaInst *Alloca = CompilerState.Builder->CreateAlloca(VarType, nullptr, Name);
        
        if (!InitVal)
        {
            if (ExprType.isArray())
                InitValIR = llvm::ConstantAggregateZero::get(VarType);
            else if (ExprType.Base == BaseType::String)
            {
                llvm::Constant *NullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*CompilerState.TheContext));
                llvm::Constant *ZeroLen = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
                InitValIR = llvm::ConstantStruct::get(CompilerState.StringStructType, {NullPtr, ZeroLen});
            }
            else if (IsRef)
                InitValIR = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(VarType)); // default pointer initializes to null (0x0)
            else
                InitValIR = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true)); // default initialize integer to 0
        }
        else
        {
            InitValIR = InitVal->codegen();
            if (!InitValIR)
            {
                if (ExprType.isArray())
                    InitValIR = llvm::ConstantAggregateZero::get(VarType);
                else if (ExprType.Base == BaseType::String)
                {
                    llvm::Constant *NullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*CompilerState.TheContext));
                    llvm::Constant *ZeroLen = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
                    InitValIR = llvm::ConstantStruct::get(CompilerState.StringStructType, {NullPtr, ZeroLen});
                }
                else
                    InitValIR = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true)); // default initialize integer to 0
            }
        }
        
        CompilerState.Builder->CreateStore(InitValIR, Alloca);
        CompilerState.NamedValues[Name] = Alloca;
        return Alloca;
    }
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

        TypeInfo declaredType(declaredBase, IsRef, ArrayCapacity);

        if (InitVal)
        {
            if (!InitVal->typeCheck(symTab))
                return false;

            if (InitVal->ExprType != declaredType)
            {
                std::cerr << "Semantic Error: Type mismatch in declaration of '" << Name
                          << "'. Expected " << declaredType.toString()
                          << ", found " << InitVal->ExprType.toString() << ".\n";
                return false;
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

    void BinaryOpAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Binary Operation: " << Op << std::endl;
        LHS->dump(indent + 1);
        RHS->dump(indent + 1);
    }
    llvm::Value *BinaryOpAST::codegen()
    {
        // Assignment needs special handling because the LHS should resolve to an address,
        // not a loaded value.
        if (Op == '=')
        {
            llvm::Value *R = RHS->codegen();
            if (!R)
                return nullptr;

            llvm::Value *VariablePtr = nullptr;

            // LHS is a standard variable
            if (auto VarAST = dynamic_cast<VariableAST *>(LHS.get()))
            {
                VariablePtr = CompilerState.NamedValues[VarAST->getName()];
                if (!VariablePtr)
                {
                    std::cerr << "Unknown variable name in assignment: " << VarAST->getName() << std::endl;
                    return nullptr;
                }
            }
            else if (auto *Deref = dynamic_cast<DerefAST *>(LHS.get()))
            {
                VariablePtr = Deref->getOperand()->codegen();
                if (!VariablePtr)
                {
                    return nullptr;
                }
            }
            else if (auto *Index = dynamic_cast<IndexAST *>(LHS.get()))
            {
                VariablePtr = Index->codegenAddress();
                if (!VariablePtr)
                    return nullptr;
            }
            else
            {
                std::cerr << "Left-hand side of assignment must be a variable, dereference, or array index." << std::endl;
                return nullptr;
            }

            CompilerState.Builder->CreateStore(R, VariablePtr);
            return R;
        }

        llvm::Value *L = LHS->codegen();
        llvm::Value *R = RHS->codegen();
        if (!L || !R)
            return nullptr; // Error in codegen of operands
        switch (Op)
        {
        // Arithemetic
        case '+':
            return CompilerState.Builder->CreateAdd(L, R, "addtmp");
        case '-':
            return CompilerState.Builder->CreateSub(L, R, "subtmp");
        case '*':
            return CompilerState.Builder->CreateMul(L, R, "multmp");
        case '/':
            return CompilerState.Builder->CreateSDiv(L, R, "divtmp");

        // Bitwise and logical
        case tok_and:
            return CompilerState.Builder->CreateAnd(L, R, "andtmp");
        case tok_or:
            return CompilerState.Builder->CreateOr(L, R, "ortmp");
        case tok_lsft:
            return CompilerState.Builder->CreateShl(L, R, "shltmp");
        case tok_rsft:
            return CompilerState.Builder->CreateAShr(L, R, "shrtmp");

        // equality
        case tok_eq:
        {
            llvm::Value *Cmp = CompilerState.Builder->CreateICmpEQ(L, R, "eqtmp");
            return CompilerState.Builder->CreateZExt(Cmp, llvm::Type::getInt32Ty(*CompilerState.TheContext), "booltmp");
        }
        case tok_neq:
        {
            llvm::Value *CmpNE = CompilerState.Builder->CreateICmpNE(L, R, "netmp");
            return CompilerState.Builder->CreateZExt(CmpNE, llvm::Type::getInt32Ty(*CompilerState.TheContext), "booltmp");
        }

        // relational
        case '<':
        {
            llvm::Value *CmpLT = CompilerState.Builder->CreateICmpSLT(L, R, "lttmp");
            return CompilerState.Builder->CreateZExt(CmpLT, llvm::Type::getInt32Ty(*CompilerState.TheContext), "booltmp");
        }
        case '>':
        {
            llvm::Value *CmpGT = CompilerState.Builder->CreateICmpSGT(L, R, "gttmp");
            return CompilerState.Builder->CreateZExt(CmpGT, llvm::Type::getInt32Ty(*CompilerState.TheContext), "booltmp");
        }

        default:
            std::cerr << "Unknown binary operator: " << Op << std::endl;
            return nullptr;
        }
    }
    bool BinaryOpAST::typeCheck(SymbolTable& symTab)
    {
        if (!LHS->typeCheck(symTab) || !RHS->typeCheck(symTab))
            return false;

        if (Op == '=')
        {
            bool isVariable = dynamic_cast<VariableAST*>(LHS.get()) != nullptr;
            bool isDeref = dynamic_cast<DerefAST*>(LHS.get()) != nullptr;
            bool isIndex = dynamic_cast<IndexAST*>(LHS.get()) != nullptr;
            
            if (!isVariable && !isDeref && !isIndex)
            {
                std::cerr << "Semantic Error: Left Hand Side of assignment must be a variable, dereference, or array index.\n";
                return false;
            }
            if (LHS->ExprType != RHS->ExprType)
            {
                std::cerr << "Semantic Error: Types of LHS and RHS must be the same.\n";
                return false;
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

    void UnaryOpAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Unary Operation: " << Op << std::endl;
        Operand->dump(indent + 1);
    }
    llvm::Value *UnaryOpAST::codegen()
    {
        llvm::Value *OperandVal = Operand->codegen();
        if (!OperandVal)
            return nullptr;
        llvm::Value *Result = nullptr;
        switch (Op)
        {
            case '-':
                Result = CompilerState.Builder->CreateNeg(OperandVal, "neg");
                break;
            default:
                std::cerr << "Unknown unary operator: " << Op << std::endl;
                return nullptr;
        }
        return Result;
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

    void BlockAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Block:" << std::endl;
        for (const auto &stmt : Statements)
        {
            stmt->dump(indent + 1);
        }
    }
    llvm::Value *BlockAST::codegen()
    {
        llvm::Value *LastVal = nullptr;
        for (const auto &stmt : Statements)
        {
            if (CompilerState.Builder->GetInsertBlock()->getTerminator())
                break;
            LastVal = stmt->codegen();
            if (!LastVal)
            {
                return nullptr;
            }
        }
        if (LastVal)
            return LastVal;
        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
    }
    bool BlockAST::typeCheck(SymbolTable& symTab)
    {
        symTab.enterScope();

        for (auto& stmt : Statements)
        {
            if (!stmt->typeCheck(symTab))
            {
                symTab.exitScope();
                return false;
            }
        }

        symTab.exitScope();
        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    void IfAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "If Statement:" << std::endl;
        printIndent(indent + 1);
        std::cout << "Condition:" << std::endl;
        Cond->dump(indent + 2);
        printIndent(indent + 1);
        std::cout << "Then:" << std::endl;
        Then->dump(indent + 2);
        if (Else)
        {
            printIndent(indent + 1);
            std::cout << "Else:" << std::endl;
            Else->dump(indent + 2);
        }
    }
    llvm::Value *IfAST::codegen()
    {
        llvm::Value *CondV = Cond->codegen();
        if (!CondV)
        {
            return nullptr;
        }

        llvm::Value *Zero = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
        llvm::Value *CondBool = CompilerState.Builder->CreateICmpNE(CondV, Zero, "ifcond");

        llvm::Function *TheFunction = CompilerState.Builder->GetInsertBlock()->getParent();

        llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "then", TheFunction);
        llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "else", TheFunction);
        llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "ifcont", TheFunction);

        CompilerState.Builder->CreateCondBr(CondBool, ThenBB, ElseBB);

        CompilerState.Builder->SetInsertPoint(ThenBB);
        llvm::Value *ThenV = Then->codegen();
        if (!ThenV)
            return nullptr;
        CompilerState.Builder->CreateBr(MergeBB);
        ThenBB = CompilerState.Builder->GetInsertBlock();

        CompilerState.Builder->SetInsertPoint(ElseBB);
        llvm::Value *ElseV = nullptr;
        if (Else)
        {
            ElseV = Else->codegen();
            if (!ElseV)
                return nullptr;
        }
        CompilerState.Builder->CreateBr(MergeBB);
        ElseBB = CompilerState.Builder->GetInsertBlock();

        CompilerState.Builder->SetInsertPoint(MergeBB);

        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
    }
    bool IfAST::typeCheck(SymbolTable& symTab)
    {
        if (!Cond->typeCheck(symTab))
            return false;
        if (!Then->typeCheck(symTab))
            return false;
        if (Else && !Else->typeCheck(symTab))
            return false;

        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    void WhileAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "While Loop:" << std::endl;
        printIndent(indent + 1);
        std::cout << "Condition:" << std::endl;
        Cond->dump(indent + 2);
        printIndent(indent + 1);
        std::cout << "Body:" << std::endl;
        Body->dump(indent + 2);
    }
    llvm::Value *WhileAST::codegen()
    {
        llvm::Function *TheFunction = CompilerState.Builder->GetInsertBlock()->getParent();

        llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "cond", TheFunction);
        llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "loop", TheFunction);
        llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "afterloop", TheFunction);

        CompilerState.Builder->CreateBr(CondBB);
        CompilerState.Builder->SetInsertPoint(CondBB);

        llvm::Value *CondV = Cond->codegen();
        if (!CondV)
            return nullptr;

        llvm::Value *Zero = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
        llvm::Value *CondBool = CompilerState.Builder->CreateICmpNE(CondV, Zero, "loopcond");

        CompilerState.Builder->CreateCondBr(CondBool, LoopBB, AfterBB);

        CompilerState.Builder->SetInsertPoint(LoopBB);
        llvm::Value *BodyV = Body->codegen();
        if (!BodyV)
            return nullptr;
        CompilerState.Builder->CreateBr(CondBB);
        CompilerState.Builder->SetInsertPoint(AfterBB);
        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
    }
    bool WhileAST::typeCheck(SymbolTable& symTab)
    {
        if (!Cond->typeCheck(symTab))
            return false;
        if (!Body->typeCheck(symTab))
            return false;

        ExprType = TypeInfo(BaseType::Int, false);
        return true;
    }

    void ForAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "For Loop" << std::endl;
        if (Kind == LoopKind::Array)
        {
            printIndent(indent + 1);
            std::cout << "Array: " << ArrayName << std::endl;
        }
        else
        {
            printIndent(indent + 1);
            std::cout << "Start: " << std::endl;
            Start->dump(indent + 2);
            printIndent(indent + 1);
            std::cout << "End: " << std::endl;
            End->dump(indent + 2);
            if (Step)
            {
                printIndent(indent + 1);
                std::cout << "Step: " << std::endl;
                Step->dump(indent + 2);
            }
        }
        printIndent(indent + 1);
        std::cout << "Body:" << std::endl;
        Body->dump(indent + 2);
    }
    llvm::Value *ForAST::codegen()
    {
        if (Kind == LoopKind::Array)
        {
            llvm::AllocaInst *ArrayAlloca = CompilerState.NamedValues[ArrayName];
            if (!ArrayAlloca)
            {
                std::cerr << "Unknown array name in for loop: " << ArrayName << std::endl;
                return nullptr;
            }

            auto *ArrayType = llvm::dyn_cast<llvm::ArrayType>(ArrayAlloca->getAllocatedType());
            if (!ArrayType)
            {
                std::cerr << "For loop source '" << ArrayName << "' is not a fixed-size array." << std::endl;
                return nullptr;
            }

            uint64_t ArrayLen = ArrayType->getNumElements();

            llvm::Function *TheFunction = CompilerState.Builder->GetInsertBlock()->getParent();
            llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "forarr.cond", TheFunction);
            llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "forarr.body", TheFunction);
            llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "forarr.after", TheFunction);

            llvm::AllocaInst *IndexAlloca = CompilerState.Builder->CreateAlloca(
                llvm::Type::getInt32Ty(*CompilerState.TheContext), nullptr, VarName + ".idx");
            llvm::AllocaInst *IterAlloca = CompilerState.Builder->CreateAlloca(
                llvm::Type::getInt32Ty(*CompilerState.TheContext), nullptr, VarName);

            llvm::Value *Zero = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
            llvm::Value *LenVal = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, ArrayLen, false));
            CompilerState.Builder->CreateStore(Zero, IndexAlloca);

            llvm::AllocaInst *OldVal = nullptr;
            auto OldValIt = CompilerState.NamedValues.find(VarName);
            if (OldValIt != CompilerState.NamedValues.end())
                OldVal = OldValIt->second;
            CompilerState.NamedValues[VarName] = IterAlloca;

            CompilerState.Builder->CreateBr(CondBB);
            CompilerState.Builder->SetInsertPoint(CondBB);

            llvm::Value *CurIdx = CompilerState.Builder->CreateLoad(
                IndexAlloca->getAllocatedType(), IndexAlloca, (VarName + ".idx.cur").c_str());
            llvm::Value *CondVal = CompilerState.Builder->CreateICmpSLT(CurIdx, LenVal, "forarr.cond");
            CompilerState.Builder->CreateCondBr(CondVal, LoopBB, AfterBB);

            CompilerState.Builder->SetInsertPoint(LoopBB);

            llvm::Value *ElemPtr = CompilerState.Builder->CreateInBoundsGEP(
                ArrayAlloca->getAllocatedType(),
                ArrayAlloca,
                {Zero, CurIdx},
                "forarr.elem.ptr");
            llvm::Value *ElemVal = CompilerState.Builder->CreateLoad(
                llvm::Type::getInt32Ty(*CompilerState.TheContext), ElemPtr, "forarr.elem");
            CompilerState.Builder->CreateStore(ElemVal, IterAlloca);

            if (!Body->codegen())
                return nullptr;

            llvm::Value *NextIdx = CompilerState.Builder->CreateAdd(
                CurIdx,
                llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 1, true)),
                "forarr.nextidx");
            CompilerState.Builder->CreateStore(NextIdx, IndexAlloca);
            CompilerState.Builder->CreateBr(CondBB);

            CompilerState.Builder->SetInsertPoint(AfterBB);
            if (OldVal)
                CompilerState.NamedValues[VarName] = OldVal;
            else
                CompilerState.NamedValues.erase(VarName);

            return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
        }

        // StartValue
        llvm::Value *StartValue = Start->codegen();
        if (!StartValue)
            return nullptr;

        // Evaluate step once for loop direction and increment.
        llvm::Value *StepVal = nullptr;
        if (Step)
        {
            StepVal = Step->codegen();
            if (!StepVal)
                return nullptr;
        }
        else
        {
            StepVal = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 1, true));
        }

        if (auto *ConstStep = llvm::dyn_cast<llvm::ConstantInt>(StepVal))
        {
            if (ConstStep->isZero())
            {
                std::cerr << "For loop step cannot be 0." << std::endl;
                return nullptr;
            }
        }

        // Basic Blocks
        llvm::Function *TheFunction = CompilerState.Builder->GetInsertBlock()->getParent();
        // llvm::BasicBlock *PreHeaderBB = CompilerState.Builder->GetInsertBlock();
        llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "forcond", TheFunction);
        llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "forloop", TheFunction);
        llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "afterfor", TheFunction);

        // Allocate loop variable and store the start value e.g. itr
        llvm::AllocaInst *Alloca = CompilerState.Builder->CreateAlloca(llvm::Type::getInt32Ty(*CompilerState.TheContext), nullptr, VarName);
        CompilerState.Builder->CreateStore(StartValue, Alloca);

        // Save current symbol binding (if any) and shadow it within the loop.
        llvm::AllocaInst *OldVal = nullptr;
        auto OldValIt = CompilerState.NamedValues.find(VarName);
        if (OldValIt != CompilerState.NamedValues.end())
            OldVal = OldValIt->second;
        CompilerState.NamedValues[VarName] = Alloca;

        // Jump to condition block
        CompilerState.Builder->CreateBr(CondBB);
        CompilerState.Builder->SetInsertPoint(CondBB);

        // Evaluate condition based on step sign.
        llvm::Value *EndVal = End->codegen();
        if (!EndVal)
            return nullptr;
        llvm::Value *CurVar = CompilerState.Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
        llvm::Value *IsNegStep = CompilerState.Builder->CreateICmpSLT(
            StepVal,
            llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true)),
            "isnegstep");
        llvm::Value *CondAsc = CompilerState.Builder->CreateICmpSLE(CurVar, EndVal, "forcmptmp_asc");
        llvm::Value *CondDesc = CompilerState.Builder->CreateICmpSGE(CurVar, EndVal, "forcmptmp_desc");
        llvm::Value *CondVal = CompilerState.Builder->CreateSelect(IsNegStep, CondDesc, CondAsc, "forcmptmp");
        CompilerState.Builder->CreateCondBr(CondVal, LoopBB, AfterBB);

        // Emit loop body
        CompilerState.Builder->SetInsertPoint(LoopBB);
        if(!Body->codegen())
            return nullptr;

        llvm::Value *CurVarForInc = CompilerState.Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
        llvm::Value *NewVarForInc = CompilerState.Builder->CreateAdd(CurVarForInc, StepVal, "nextvar");
        CompilerState.Builder->CreateStore(NewVarForInc, Alloca);

        // loop back to condition
        CompilerState.Builder->CreateBr(CondBB);

        // Cleanup and exit
        CompilerState.Builder->SetInsertPoint(AfterBB);

        // Restore previous symbol binding after loop scope ends.
        if (OldVal)
            CompilerState.NamedValues[VarName] = OldVal;
        else
            CompilerState.NamedValues.erase(VarName);

        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
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
    

    void CallAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Function Call: " << Callee << std::endl;
        for (const auto &arg : Args)
        {
            arg->dump(indent + 1);
        }
    }
    llvm::Value *CallAST::codegen()
    {
        llvm::Function *CalleeF = CompilerState.TheModule->getFunction(Callee);
        if (!CalleeF)
        {
            std::cerr << "Unknown function referenced: " << Callee << std::endl;
            return nullptr;
        }

        if (CalleeF->arg_size() != Args.size())
        {
            std::cerr << "Incorrect number of arguments passed to function: " << Callee << std::endl;
            return nullptr;
        }

        std::vector<llvm::Value *> ArgsV;
        for (unsigned i = 0, e = Args.size(); i != e; ++i)
        {
            llvm::Value *ArgV = Args[i]->codegen();
            if (!ArgV)
                return nullptr;
            ArgsV.push_back(ArgV);
        }

        return CompilerState.Builder->CreateCall(CalleeF, ArgsV, "calltmp");
    }
    bool CallAST::typeCheck(SymbolTable& symTab)
    {
        auto toLLVMType = [&](const TypeInfo &T) -> llvm::Type *
        {
            if (!CompilerState.TheContext)
                return nullptr;

            if (T.isArray())
            {
                return llvm::ArrayType::get(llvm::Type::getInt32Ty(*CompilerState.TheContext), T.ArrayCapacity);
            }
            if (T.Base == BaseType::String)
            {
                return CompilerState.StringStructType;
            }
            if (T.IsRef)
            {
                return llvm::PointerType::getUnqual(*CompilerState.TheContext);
            }
            if (T.Base == BaseType::Int)
            {
                return llvm::Type::getInt32Ty(*CompilerState.TheContext);
            }
            if (T.Base == BaseType::Void)
            {
                return llvm::Type::getVoidTy(*CompilerState.TheContext);
            }
            return nullptr;
        };

        if (!CompilerState.TheModule)
        {
            std::cerr << "Semantic Error: Compiler module is not initialized for function call checks.\n";
            return false;
        }

        llvm::Function *CalleeF = CompilerState.TheModule->getFunction(Callee);
        if (!CalleeF)
        {
            std::cerr << "Semantic Error: Unknown function '" << Callee << "'.\n";
            return false;
        }

        if (CalleeF->arg_size() != Args.size())
        {
            std::cerr << "Semantic Error: Incorrect number of arguments passed to function '" << Callee << "'.\n";
            return false;
        }

        unsigned i = 0;
        for (const auto &Arg : Args)
        {
            if (!Arg->typeCheck(symTab))
                return false;

            llvm::Type *ExpectedTy = CalleeF->getFunctionType()->getParamType(i);
            llvm::Type *ActualTy = toLLVMType(Arg->ExprType);
            if (!ActualTy || ActualTy != ExpectedTy)
            {
                std::string ExpectedTypeStr;
                if (ExpectedTy->isIntegerTy(32))
                    ExpectedTypeStr = "int";
                else if (ExpectedTy->isVoidTy())
                    ExpectedTypeStr = "void";
                else if (CompilerState.StringStructType && ExpectedTy == CompilerState.StringStructType)
                    ExpectedTypeStr = "str";
                else if (ExpectedTy->isPointerTy())
                    ExpectedTypeStr = "ref";
                else
                    ExpectedTypeStr = "unknown";

                std::cerr << "Semantic Error: Type mismatch for argument " << i
                          << " in call to '" << Callee << "'. Expected "
                          << ExpectedTypeStr << ", found " << Arg->ExprType.toString() << ".\n";
                return false;
            }
            ++i;
        }

        llvm::Type *RetTy = CalleeF->getReturnType();
        if (RetTy->isIntegerTy(32))
            ExprType = TypeInfo(BaseType::Int, false);
        else if (CompilerState.StringStructType && RetTy == CompilerState.StringStructType)
            ExprType = TypeInfo(BaseType::String, false);
        else if (RetTy->isVoidTy())
            ExprType = TypeInfo(BaseType::Void, false);
        else
            ExprType = TypeInfo(BaseType::Unknown, false);

        return true;
    }

    void ReturnAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Return" << std::endl;
        if (RetVal)
            RetVal->dump(indent + 1);
    }
    llvm::Value *ReturnAST::codegen()
    {
        if (!RetVal)
            return CompilerState.Builder->CreateRetVoid();

        llvm::Value *RetIR = RetVal->codegen();
        if (!RetIR)
            return nullptr;
        return CompilerState.Builder->CreateRet(RetIR);
    }
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
        return true;
    }

    void FunctionAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Function: " << Name << " -> " << ReturnType << std::endl;
        printIndent(indent + 1);
        std::cout << "Params:" << std::endl;
        for (const auto &P : Params)
        {
            printIndent(indent + 2);
            std::cout << P.TypeName << (P.IsRef ? " ref " : " ") << P.Name << std::endl;
        }
        printIndent(indent + 1);
        std::cout << "Body:" << std::endl;
        Body->dump(indent + 2);
    }
    llvm::Value *FunctionAST::codegen()
    {
        auto toLLVMType = [&](const std::string &typeName, bool isRef) -> llvm::Type *
        {
            if (typeName == "int")
            {
                if (isRef)
                    return llvm::PointerType::getUnqual(*CompilerState.TheContext);
                return llvm::Type::getInt32Ty(*CompilerState.TheContext);
            }
            if (typeName == "str")
                return CompilerState.StringStructType;
            if (typeName == "void")
                return llvm::Type::getVoidTy(*CompilerState.TheContext);
            return nullptr;
        };

        std::vector<llvm::Type *> ArgTypes;
        ArgTypes.reserve(Params.size());
        for (const auto &P : Params)
        {
            llvm::Type *ParamTy = toLLVMType(P.TypeName, P.IsRef);
            if (!ParamTy)
                return nullptr;
            ArgTypes.push_back(ParamTy);
        }

        llvm::Type *RetTy = toLLVMType(ReturnType, false);
        if (!RetTy)
            return nullptr;

        llvm::Function *Fn = CompilerState.TheModule->getFunction(Name);
        if (!Fn)
        {
            llvm::FunctionType *FnTy = llvm::FunctionType::get(RetTy, ArgTypes, false);
            Fn = llvm::Function::Create(FnTy, llvm::Function::ExternalLinkage, Name, CompilerState.TheModule.get());
        }

        if (!Fn->empty())
        {
            std::cerr << "Function redefinition is not allowed: " << Name << std::endl;
            return nullptr;
        }

        llvm::BasicBlock *FnEntry = llvm::BasicBlock::Create(*CompilerState.TheContext, "entry", Fn);
        llvm::BasicBlock *OldInsertBlock = CompilerState.Builder->GetInsertBlock();
        auto OldNamedValues = CompilerState.NamedValues;
        CompilerState.NamedValues.clear();
        CompilerState.Builder->SetInsertPoint(FnEntry);

        unsigned idx = 0;
        for (auto &Arg : Fn->args())
        {
            Arg.setName(Params[idx].Name);
            llvm::AllocaInst *Alloca = CompilerState.Builder->CreateAlloca(Arg.getType(), nullptr, Params[idx].Name);
            CompilerState.Builder->CreateStore(&Arg, Alloca);
            CompilerState.NamedValues[Params[idx].Name] = Alloca;
            ++idx;
        }

        if (!Body->codegen())
            return nullptr;

        if (!CompilerState.Builder->GetInsertBlock()->getTerminator())
        {
            if (RetTy->isVoidTy())
                CompilerState.Builder->CreateRetVoid();
            else if (RetTy->isIntegerTy(32))
                CompilerState.Builder->CreateRet(llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true)));
            else if (CompilerState.StringStructType && RetTy == CompilerState.StringStructType)
            {
                llvm::Constant *NullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*CompilerState.TheContext));
                llvm::Constant *ZeroLen = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
                CompilerState.Builder->CreateRet(llvm::ConstantStruct::get(CompilerState.StringStructType, {NullPtr, ZeroLen}));
            }
        }

        CompilerState.NamedValues = OldNamedValues;
        if (OldInsertBlock)
            CompilerState.Builder->SetInsertPoint(OldInsertBlock);

        return Fn;
    }
    bool FunctionAST::typeCheck(SymbolTable& symTab)
    {
        auto toTypeInfo = [&](const std::string &typeName, bool isRef = false) -> TypeInfo
        {
            if (typeName == "int")
                return TypeInfo(BaseType::Int, isRef);
            if (typeName == "str")
                return TypeInfo(BaseType::String, isRef);
            if (typeName == "void")
                return TypeInfo(BaseType::Void, false);
            return TypeInfo(BaseType::Unknown, false);
        };

        if (!CompilerState.TheModule)
        {
            std::cerr << "Semantic Error: Compiler module is not initialized for function checks.\n";
            return false;
        }

        std::vector<llvm::Type *> ArgTypes;
        ArgTypes.reserve(Params.size());
        for (const auto &P : Params)
        {
            TypeInfo TI = toTypeInfo(P.TypeName, P.IsRef);
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

            if (TI.Base == BaseType::Int && TI.IsRef)
                ArgTypes.push_back(llvm::PointerType::getUnqual(*CompilerState.TheContext));
            else if (TI.Base == BaseType::Int)
                ArgTypes.push_back(llvm::Type::getInt32Ty(*CompilerState.TheContext));
            else if (TI.Base == BaseType::String)
                ArgTypes.push_back(CompilerState.StringStructType);
        }

        TypeInfo RetInfo = toTypeInfo(ReturnType, false);
        if (RetInfo.Base == BaseType::Unknown)
        {
            std::cerr << "Semantic Error: Unknown return type '" << ReturnType << "' in function '" << Name << "'.\n";
            return false;
        }

        llvm::Type *RetTy = nullptr;
        if (RetInfo.Base == BaseType::Int)
            RetTy = llvm::Type::getInt32Ty(*CompilerState.TheContext);
        else if (RetInfo.Base == BaseType::String)
            RetTy = CompilerState.StringStructType;
        else
            RetTy = llvm::Type::getVoidTy(*CompilerState.TheContext);

        llvm::Function *Existing = CompilerState.TheModule->getFunction(Name);
        if (!Existing)
        {
            llvm::FunctionType *FnTy = llvm::FunctionType::get(RetTy, ArgTypes, false);
            llvm::Function::Create(FnTy, llvm::Function::ExternalLinkage, Name, CompilerState.TheModule.get());
        }

        symTab.enterScope();
        for (const auto &P : Params)
        {
            TypeInfo TI = toTypeInfo(P.TypeName, P.IsRef);
            if (!symTab.insert(P.Name, TI, true))
            {
                std::cerr << "Semantic Error: Duplicate parameter name '" << P.Name << "' in function '" << Name << "'.\n";
                symTab.exitScope();
                return false;
            }
        }

        FunctionReturnTypeStack.push_back(RetInfo);
        bool BodyOk = Body->typeCheck(symTab);
        FunctionReturnTypeStack.pop_back();
        symTab.exitScope();

        ExprType = TypeInfo(BaseType::Void, false);
        return BodyOk;
    }

    void ImportAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Import: " << ModuleName << std::endl;
    }
    llvm::Value *ImportAST::codegen()
    {
        for (const auto &Node : ImportedNodes)
        {
            if (!Node->codegen())
            {
                std::cerr << "Failed to generate IR for imprted module." << ModuleName << std::endl;
                return nullptr;
            }
        }

        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
    }
    bool ImportAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        return true;
    }

    void AddressOfAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "AddressOf: " << VarName << std::endl;
    }
    llvm::Value *AddressOfAST::codegen()
    {
        llvm::AllocaInst *Alloca = CompilerState.NamedValues[VarName];
        if (!Alloca)
        {
            std::cerr << "Unknown Variable: " << VarName << std::endl;
            return nullptr;
        }
        return Alloca;
    }
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

    void DerefAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Deref: " << std::endl;
        Operand->dump(indent + 1);
    }
    llvm::Value *DerefAST::codegen()
    {
        llvm::Value *PtrValue = Operand->codegen();
        if (!PtrValue)
            return nullptr;
        return CompilerState.Builder->CreateLoad(llvm::Type::getInt32Ty(*CompilerState.TheContext), PtrValue, "dereftmp");
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
 
    void IndexAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Index: " << ArrayName << "[" << std::endl;
        IndexExpr->dump(indent + 1);
    }
    llvm::Value *IndexAST::codegenAddress()
    {
        // Get starting point of the array from symbol table
        llvm::AllocaInst *ArrayPtr = CompilerState.NamedValues[ArrayName];
        if (!ArrayPtr)
            return nullptr;

        // Evaluate the index expression
        llvm::Value *IndexVal = IndexExpr->codegen();
        if (!IndexVal)
            return nullptr;

        // first index steps into the array object, second index selects element
        llvm::Value *Zero = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
        std::vector<llvm::Value *> Indices = {Zero, IndexVal};
        return CompilerState.Builder->CreateInBoundsGEP(
            ArrayPtr->getAllocatedType(),
            ArrayPtr,
            Indices,
            "arrayidx.ptr");
    }
    llvm::Value *IndexAST::codegen()
    {
        llvm::Value *ElementPtr = codegenAddress();
        if (!ElementPtr)
            return nullptr;
        return CompilerState.Builder->CreateLoad(llvm::Type::getInt32Ty(*CompilerState.TheContext), ElementPtr, "idxload");
    }
    bool IndexAST::typeCheck(SymbolTable& symTab)
    {
        // Verify if the variable exists in symbl table
        Symbol* sym = symTab.lookup(ArrayName);
        if (!sym)
        {
            std::cerr << "Semantic Error: Array '" << ArrayName << "' is not declared.\n";
            return false;
        }

        // Verify if its an array
        if (!sym->Type.isArray())
        {
            std::cerr << "Semantic Error: Variable '" << ArrayName << "' is not an array.\n";
            return false;
        }

        // Verify if the index expression is valid and resolves to an integer
        if (!IndexExpr->typeCheck(symTab))
            return false;
        if (IndexExpr->ExprType.Base != BaseType::Int || IndexExpr->ExprType.IsRef || IndexExpr->ExprType.isArray())
        {
            std::cerr << "Semantic Error: Array '" << ArrayName << "' must be of type int.\n";
            return false;
        }

        // Compile time bounds checking
        // If the index is hardcooded number we can check it right now
        if (auto* NumNode = dynamic_cast<NumberAST*>(IndexExpr.get()))
        {
            int indexValue = NumNode->getVal();
            if (indexValue < 0 || indexValue >= sym->Type.ArrayCapacity)
            {
                std::cerr << "Semantic Error: Array index out of bounds. '" << ArrayName << "' has capacity " << sym->Type.ArrayCapacity << ", but accessed at index " << indexValue << ".\n";
                return false; // Halts compilation immediately
            }
        }

        // return type indexing int[10] -> int
        ExprType = TypeInfo(sym->Type.Base, false);
        return true;
    }

    void StringLiteralAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "StringLiteral: \"" << Val << "\"\n";
    }
    llvm::Value *StringLiteralAST::codegen()
    {
        // Create the raw charecter array constant in llvm without auto apppending
        llvm::Constant *StrConstant = llvm::ConstantDataArray::getString(*CompilerState.TheContext, Val, false);
        // Create a global variable to hold the string constant
        llvm::GlobalVariable *StrVar = new llvm::GlobalVariable(*CompilerState.TheModule, StrConstant->getType(), true, llvm::GlobalValue::PrivateLinkage, StrConstant, ".str.literal");
        // create the fat pointer struct : {ptr, len}
        llvm::Constant *Ptr = StrVar;
        llvm::Constant *Len = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, Val.length(), true));
        
        return llvm::ConstantStruct::get(CompilerState.StringStructType, {Ptr, Len});
    }
    bool StringLiteralAST::typeCheck(SymbolTable& symTab)
    {
        (void)symTab;
        // mark this node as a string type natively
        ExprType = TypeInfo(BaseType::String, false);
        return true;
    }
}

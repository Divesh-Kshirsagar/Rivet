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

    /**
     * @brief Converts a TypeInfo's base into the matching scalar LLVM type.
     *
     * Used by memory operations (DerefAST, IndexAST, ForAST) to avoid hardcoding
     * getInt32Ty and to correctly handle future non-int element types.
     */
    static llvm::Type *toLLVMElementType(const TypeInfo &TI)
    {
        if (TI.IsRef || TI.IsOptRef)
            return llvm::PointerType::getUnqual(*CompilerState.TheContext);
        if (TI.Base == BaseType::String)
            return CompilerState.StringStructType;
        // Default: Int (and Unknown fall through here as a safe fallback)
        return llvm::Type::getInt32Ty(*CompilerState.TheContext);
    }

    void NumberAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Number: " << Val << std::endl;
    }
    llvm::Value *NumberAST::codegen()
    {
        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, Val, true));
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
        if (!L) return nullptr;

        // Short-circuit AND: if LHS is false (0), skip RHS entirely.
        if (Op == tok_and)
        {
            llvm::Function *Fn = CompilerState.Builder->GetInsertBlock()->getParent();
            llvm::Value *Zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*CompilerState.TheContext), 0);
            llvm::Value *LBool = CompilerState.Builder->CreateICmpNE(L, Zero, "and.lhs");

            llvm::BasicBlock *RhsBB  = llvm::BasicBlock::Create(*CompilerState.TheContext, "and.rhs",  Fn);
            llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "and.merge", Fn);

            // If LHS == false → jump to merge with 0; else evaluate RHS
            CompilerState.Builder->CreateCondBr(LBool, RhsBB, MergeBB);
            llvm::BasicBlock *LhsEndBB = CompilerState.Builder->GetInsertBlock();

            // RHS block
            CompilerState.Builder->SetInsertPoint(RhsBB);
            llvm::Value *R = RHS->codegen();
            if (!R) return nullptr;
            llvm::Value *RBool = CompilerState.Builder->CreateICmpNE(R, Zero, "and.rhs.bool");
            llvm::Value *RInt  = CompilerState.Builder->CreateZExt(RBool, llvm::Type::getInt32Ty(*CompilerState.TheContext), "and.rhs.int");
            CompilerState.Builder->CreateBr(MergeBB);
            llvm::BasicBlock *RhsEndBB = CompilerState.Builder->GetInsertBlock();

            // Merge block — PHI selects 0 (short-circuit) or RHS result
            CompilerState.Builder->SetInsertPoint(MergeBB);
            llvm::PHINode *Phi = CompilerState.Builder->CreatePHI(
                llvm::Type::getInt32Ty(*CompilerState.TheContext), 2, "and.result");
            Phi->addIncoming(Zero,  LhsEndBB);   // LHS was false → 0
            Phi->addIncoming(RInt,  RhsEndBB);   // LHS was true  → RHS value
            return Phi;
        }

        // Short-circuit OR: if LHS is true (!= 0), skip RHS entirely.
        if (Op == tok_or)
        {
            llvm::Function *Fn = CompilerState.Builder->GetInsertBlock()->getParent();
            llvm::Value *Zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*CompilerState.TheContext), 0);
            llvm::Value *One  = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*CompilerState.TheContext), 1);
            llvm::Value *LBool = CompilerState.Builder->CreateICmpNE(L, Zero, "or.lhs");

            llvm::BasicBlock *RhsBB  = llvm::BasicBlock::Create(*CompilerState.TheContext, "or.rhs",  Fn);
            llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "or.merge", Fn);

            // If LHS == true → jump to merge with 1; else evaluate RHS
            CompilerState.Builder->CreateCondBr(LBool, MergeBB, RhsBB);
            llvm::BasicBlock *LhsEndBB = CompilerState.Builder->GetInsertBlock();

            // RHS block
            CompilerState.Builder->SetInsertPoint(RhsBB);
            llvm::Value *R = RHS->codegen();
            if (!R) return nullptr;
            llvm::Value *RBool = CompilerState.Builder->CreateICmpNE(R, Zero, "or.rhs.bool");
            llvm::Value *RInt  = CompilerState.Builder->CreateZExt(RBool, llvm::Type::getInt32Ty(*CompilerState.TheContext), "or.rhs.int");
            CompilerState.Builder->CreateBr(MergeBB);
            llvm::BasicBlock *RhsEndBB = CompilerState.Builder->GetInsertBlock();

            // Merge block — PHI selects 1 (short-circuit) or RHS result
            CompilerState.Builder->SetInsertPoint(MergeBB);
            llvm::PHINode *Phi = CompilerState.Builder->CreatePHI(
                llvm::Type::getInt32Ty(*CompilerState.TheContext), 2, "or.result");
            Phi->addIncoming(One,  LhsEndBB);    // LHS was true  → 1
            Phi->addIncoming(RInt, RhsEndBB);    // LHS was false → RHS value
            return Phi;
        }

        llvm::Value *R = RHS->codegen();
        if (!R) return nullptr;

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

        // Bitwise shifts (these are fine to evaluate both sides always)
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
        // Snapshot outer scope bindings. Any variable declared inside this block
        // will overwrite entries in NamedValues, but we restore them on exit so
        // the outer scope is never polluted by inner declarations.
        auto SavedNamedValues = CompilerState.NamedValues;

        llvm::Value *LastVal = nullptr;
        for (const auto &stmt : Statements)
        {
            if (CompilerState.Builder->GetInsertBlock()->getTerminator())
                break;
            LastVal = stmt->codegen();
            if (!LastVal)
            {
                CompilerState.NamedValues = SavedNamedValues;
                return nullptr;
            }
        }

        CompilerState.NamedValues = SavedNamedValues;

        if (LastVal)
            return LastVal;
        return llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true));
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
            // Derive element type from the array's declared element base type
            // rather than hardcoding Int32, so future element types work correctly.
            llvm::Type *ElemTy = toLLVMElementType(TypeInfo(ArrayType->getElementType() == llvm::Type::getInt32Ty(*CompilerState.TheContext) ? BaseType::Int : BaseType::Unknown, false));
            llvm::Value *ElemVal = CompilerState.Builder->CreateLoad(ElemTy, ElemPtr, "forarr.elem");
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

    void FunctionAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "Function: " << Name << " -> " << ReturnType << std::endl;
        printIndent(indent + 1);
        std::cout << "Params:" << std::endl;
        for (const auto &P : Params)
        {
            printIndent(indent + 2);
            std::cout << P.TypeName << (P.IsRef ? (P.IsOptRef ? " optref " : " ref ") : " ") << P.Name << std::endl;
        }
        printIndent(indent + 1);
        std::cout << "Body:" << std::endl;
        Body->dump(indent + 2);
    }
    llvm::Function *FunctionAST::createPrototype()
    {
        auto toLLVMType = [&](const std::string &typeName, bool isRef) -> llvm::Type *
        {
            if (typeName == "int")
            {
                if (isRef) return llvm::PointerType::getUnqual(*CompilerState.TheContext);
                return llvm::Type::getInt32Ty(*CompilerState.TheContext);
            }
            if (typeName == "str")
                return CompilerState.StringStructType;
            if (typeName == "void")
                return llvm::Type::getVoidTy(*CompilerState.TheContext);
            return nullptr;
        };

        // Skip if already declared
        if (llvm::Function *Existing = CompilerState.TheModule->getFunction(Name))
            return Existing;

        std::vector<llvm::Type *> ArgTypes;
        for (const auto &P : Params)
        {
            llvm::Type *T = toLLVMType(P.TypeName, P.IsRef);
            if (!T) return nullptr;
            ArgTypes.push_back(T);
        }
        llvm::Type *RetTy = toLLVMType(ReturnType, false);
        if (!RetTy) return nullptr;

        llvm::FunctionType *FnTy = llvm::FunctionType::get(RetTy, ArgTypes, false);
        return llvm::Function::Create(FnTy, llvm::Function::ExternalLinkage, Name, CompilerState.TheModule.get());
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
        // Use the type-checker's resolved ExprType (set by DerefAST::typeCheck)
        // so this load is always correct regardless of what the pointer points to.
        llvm::Type *LoadTy = toLLVMElementType(ExprType);
        return CompilerState.Builder->CreateLoad(LoadTy, PtrValue, "dereftmp");
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
        // Use the type-checker's resolved ExprType (set by IndexAST::typeCheck)
        // so element loads are correct for any future array element types.
        llvm::Type *ElemTy = toLLVMElementType(ExprType);
        return CompilerState.Builder->CreateLoad(ElemTy, ElementPtr, "idxload");
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

    void NullLiteralAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "NullLiteral\n";
    }
    llvm::Value *NullLiteralAST::codegen()
    {
        // Emit a null pointer constant. optref variables store ptr-typed allocas,
        // so a null ptr constant is the correct LLVM representation.
        return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*CompilerState.TheContext));
    }
}

#include "Rivet/AST.h"
#include "Rivet/Lexer.h"
#include "Rivet/CodeGen.h"
#include <iostream>
#include <llvm/IR/Constant.h>

namespace Rivet
{
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
        if (IsRef)
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
            if (IsRef)
                InitValIR = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(VarType)); // default pointer initializes to null (0x0)
            else
                InitValIR = llvm::ConstantInt::get(*CompilerState.TheContext, llvm::APInt(32, 0, true)); // default initialize integer to 0
        }
        else
        {
            InitValIR = InitVal->codegen();
            if (!InitValIR)
                return nullptr;
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

        // assignment
        case '=':
        {
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
            else
            {
                std::cerr << "Left-hand side of assignment must be a variable." << std::endl;
                return nullptr;
            }
            
            CompilerState.Builder->CreateStore(R, VariablePtr);
            return R;
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
        llvm::Value *LastVal = nullptr;
        for (const auto &stmt : Statements)
        {
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

    // TODO: Implement the for loop for arrays
    void ForAST::dump(int indent) const
    {
        printIndent(indent);
        std::cout << "For Loop" << std::endl;
        printIndent(indent + 1);
        // std::cout << "Itr: " << Itr << std::endl;
        // printIndent(indent + 1);
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
        printIndent(indent + 1);
        std::cout << "Body:" << std::endl;
        Body->dump(indent + 2);
    }
    llvm::Value *ForAST::codegen()
    {
        
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
        return CompilerState.Builder->CreateLoad(llvm::Type::getInt32Ty(*CompilerState.TheContext), PtrValue, "dereftmp");
    }
    
}

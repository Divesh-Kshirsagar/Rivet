#include "Rivet/CodeGen.h"

namespace Rivet
{
    CodeGenContext CompilerState;

    void CodeGenContext::Initialize()
    {
        TheContext = std::make_unique<llvm::LLVMContext>();
        Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
        TheModule = std::make_unique<llvm::Module>("Rivet Bare Metal Module", *TheContext);

        // fat pointer string struct type
        StringStructType = llvm::StructType::create(*TheContext, "String");
        StringStructType->setBody({
            llvm::PointerType::getUnqual(*TheContext), // pointer to char
            llvm::Type::getInt32Ty(*TheContext), // length
        });

        // 
    }

}

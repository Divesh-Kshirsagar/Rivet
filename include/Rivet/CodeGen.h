/**
 * @file CodeGen.h
 * @brief Defines the globals and context structures required for LLVM IR code generation.
 * 
 * Provides the integration layer with LLVM components like Context, Builder, and
 * Module abstractions so AST nodes can synthesize code outputs dynamically.
 */
#ifndef RIVET_CODEGEN_H
#define RIVET_CODEGEN_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <map>

namespace Rivet
{
    /**
     * @struct CodeGenContext
     * @brief A bundled environment holding LLVM core objects required during IR construction.
     * 
     * Keeps track of active modules, basic block instruction builders, and maps memory
     * allocations (like stack pointers for variables) required while stepping through nodes.
     */
    struct CodeGenContext
    {
        std::unique_ptr<llvm::LLVMContext> TheContext; ///< Core LLVM environment and type management container.
        std::unique_ptr<llvm::IRBuilder<>> Builder;    ///< Assistant for generating LLVM IR instructions gracefully.
        std::unique_ptr<llvm::Module> TheModule;       ///< Contains the ultimate target code for functions and global variables.

        std::map<std::string, llvm::AllocaInst *> NamedValues; ///< Tracks memory references resolving source identifiers to LLVM stack allocations.

        llvm::StructType *StringStructType = nullptr; ///< Cached reference to the globally available fat pointer string structure.
        
        /**
         * @brief Fully instantiates LLVM internals and custom core types.
         */
        void Initialize();
    };

    /**
     * @brief The global code generation context singleton instance.
     * 
     * Exposes the global compiler state for codegen, which includes the LLVM context, 
     * IR builder, and module. This is used by the generic codegen() methods of AST 
     * nodes to generate their specific LLVM IR payloads.
     */
    extern CodeGenContext CompilerState;
}

#endif

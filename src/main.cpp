#include <iostream>
#include <string>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include "Rivet/Lexer.h"
#include "Rivet/Parser.h"
#include "Rivet/CodeGen.h"

using namespace Rivet;

// TODO: Try to use better error messages cause the current ones are pretty bad, especially for syntax errors. Maybe include line/column info in the error messages? --- IGNORE ---
int main(int argc, char** argv) {
    
    if (argc < 2) {
        std::cerr << "Fatal Error: No input file provided.\n";
        std::cerr << "Usage: rivet <source_file.rvt> [--dump-ast]\n";
        return 1;
    }

    std::string filename = argv[1];
    bool dumpAST = false;
    if (argc >= 3 && std::string(argv[2]) == "--dump-ast") {
        dumpAST = true;
    }

    std::cout << "Rivet Compiler - Compiling " << filename << "...\n";

    Lexer lexer(filename);
    Parser parser(lexer);

    auto astNodes = parser.ParseFile();

    if(astNodes.empty()) {
        std::cerr << "Error: No AST nodes generated. Compilation failed.\n";
        return 1;
    }

    // --- SEMANTIC ANALYSIS PASS ---
    SymbolTable symTab;
    int semanticErrors = 0;

    std::cout << "Running Semantic Analysis...\n";
    for (const auto& node : astNodes) {
        if (!node->typeCheck(symTab)) {
            semanticErrors++;
        }
    }

    if (semanticErrors > 0) {
        std::cerr << "\nCompilation aborted due to " << semanticErrors << " semantic error(s).\n";
        return 1;
    }


    if (dumpAST) {
        std::cout << "\n============================\n";
        std::cout << "       ABSTRACT SYNTAX TREE   \n";
        std::cout << "\n============================\n";


        for (const auto& node : astNodes) {
            node->dump(0);
        }

        std::cout << "\n============================\n";

    }

    // These are just placeholders for now, as codegen is not entirely implemented yet
    if(parser.ErrorCount > 0) {
        std::cerr << "Compilation failed with " << parser.ErrorCount << " error(s).\n";
        return 1;
    }

    CompilerState.Initialize();
    std::cout << "Rivet Compiler initialized.\n";

    auto* Int32Ty = llvm::Type::getInt32Ty(*CompilerState.TheContext);
    auto* EntryFnTy = llvm::FunctionType::get(Int32Ty, false);
    auto* EntryFn = llvm::Function::Create(
        EntryFnTy,
        llvm::Function::ExternalLinkage,
        "__rivet_entry",
        CompilerState.TheModule.get());
    auto* EntryBB = llvm::BasicBlock::Create(*CompilerState.TheContext, "entry", EntryFn);
    CompilerState.Builder->SetInsertPoint(EntryBB);

    llvm::Value* lastVal = llvm::ConstantInt::get(Int32Ty, 0, true);

    for (const auto& node : astNodes) {
        if(llvm::Value* val = node->codegen()) {
            lastVal = val;
        } else {
            std::cerr << "Error: Code generation failed for an AST node.\n";
            return 1;
        }
    }

    if (!CompilerState.Builder->GetInsertBlock()->getTerminator()) {
        if (lastVal && lastVal->getType()->isIntegerTy(32)) {
            CompilerState.Builder->CreateRet(lastVal);
        } else {
            CompilerState.Builder->CreateRet(llvm::ConstantInt::get(Int32Ty, 0, true));
        }
    }
    

    std::cout << "============================\n";
    std::cout << "           LLVM IR          \n";
    std::cout << "============================\n";
    CompilerState.TheModule->print(llvm::outs(), nullptr);
    std::cout << "============================\n";
    return 0;
}

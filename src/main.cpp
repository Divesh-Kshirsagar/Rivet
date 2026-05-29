/**
 * @file main.cpp
 * @brief Entry point for the Rivet compiler.
 * This file contains the main execution flow of the compiler, tying together
 * the Lexer, Parser, Semantic Analysis, and Code Generation phases.
 */

#include <cstdint>
#include <iostream>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <optional>
#include <sstream>
#include <string>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include "Rivet/CodeGen.h"
#include "Rivet/Lexer.h"
#include "Rivet/Parser.h"

using namespace Rivet;

/**
 * @brief The main entry point of the Rivet compiler.
 * *Invokes the lexical analysis, parsing, semantic analysis, and LLVM-based
 * code generation phases on the given Rivet source file.
 * *@param argc Number of command-line arguments.
 * @param argv Array of command-line arguments. Expected format: `rivet <source_file.rvt> [--dump-ast]
 * [--target=<triple>] [--mcpu=<cpu>] [--flash-origin=<addr>] [--flash-size=<size>] [--ram-origin=<addr>]
 * [--ram-size=<size>]`
 * @return int Returns 0 on successful compilation, 1 on failure (e.g., syntax/semantic errors or missing input).
 */
/// @todo Improve diagnostics quality for parse/codegen errors and surface
/// line/column context consistently across all failure paths.
int main ( int argc, char **argv )
{
    auto printUsage = [] ()
    {
        std::cout << "Usage: rivet <source_file.rvt> [options]\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help                 Show this help message and exit\n";
        std::cout << "  --dump-ast                 Print the parsed AST\n";
        std::cout << "  --target=<triple>          LLVM target triple (default: host triple)\n";
        std::cout << "  --mcpu=<cpu>               Target CPU name (default: generic)\n";
        std::cout << "  --flash-origin=<addr>      Flash origin address (default: 0x08000000)\n";
        std::cout << "  --flash-size=<size>        Flash size (default: 512K)\n";
        std::cout << "  --ram-origin=<addr>        RAM origin address (default: 0x20000000)\n";
        std::cout << "  --ram-size=<size>          RAM size (default: 128K)\n";
    };

    if ( argc >= 2 )
    {
        std::string firstArg = argv[1];
        if ( firstArg == "-h" || firstArg == "--help" )
        {
            printUsage ();
            return 0;
        }
    }

    if ( argc < 2 )
    {
        std::cerr << "Fatal Error: No input file provided.\n";
        printUsage ();
        return 1;
    }

    // Initialize all LLVM targets for flexible backend compilation
    llvm::InitializeAllTargetInfos ();
    llvm::InitializeAllTargets ();
    llvm::InitializeAllTargetMCs ();
    llvm::InitializeAllAsmParsers ();
    llvm::InitializeAllAsmPrinters ();

    std::string filename = argv[1];
    bool dumpAST = false;

    // Flexible architecture defaults
    std::string targetTriple = llvm::sys::getDefaultTargetTriple ();
    std::string cpu = "generic";

    // Bare-metal memory layout defaults (STM32F4-class; overridable via CLI)
    std::string flashOrigin = "0x08000000";
    std::string flashSize = "512K";
    std::string ramOrigin = "0x20000000";
    std::string ramSize = "128K";

    // Parse CLI arguments
    for ( int i = 2; i < argc; ++i )
    {
        std::string arg = argv[i];
        if ( arg == "--dump-ast" )
            dumpAST = true;
        else if ( arg.find ( "--target=" ) == 0 )
            targetTriple = arg.substr ( 9 );
        else if ( arg.find ( "--mcpu=" ) == 0 )
            cpu = arg.substr ( 7 );
        else if ( arg.find ( "--flash-origin=" ) == 0 )
            flashOrigin = arg.substr ( 15 );
        else if ( arg.find ( "--flash-size=" ) == 0 )
            flashSize = arg.substr ( 13 );
        else if ( arg.find ( "--ram-origin=" ) == 0 )
            ramOrigin = arg.substr ( 13 );
        else if ( arg.find ( "--ram-size=" ) == 0 )
            ramSize = arg.substr ( 11 );
    }

    std::cout << "Rivet Compiler - Compiling " << filename << " for target " << targetTriple << "...\n";

    Lexer lexer ( filename );
    Parser parser ( lexer );

    auto astNodes = parser.ParseFile ();

    if ( parser.ErrorCount > 0 )
    {
        std::cerr << "Compilation failed with " << parser.ErrorCount << " error(s).\n";
        return 1;
    }

    if ( astNodes.empty () )
    {
        std::cerr << "Error: No AST nodes generated. Compilation failed.\n";
        return 1;
    }

    // Initialize compiler state (required for codegen later, not for type-checking).
    CompilerState.Initialize ();

    // --- SEMANTIC PASS 1: Register all function signatures ---
    // Calling registerSignature() on every FunctionAST node populates
    // symTab.FunctionRegistry with pure TypeInfo — NO LLVM IR created.
    // This enables forward declarations: a function can be called anywhere
    // in the file regardless of source order.
    SymbolTable symTab;
    int semanticErrors = 0;

    std::cout << "Running Semantic Analysis...\n";
    for ( const auto &node : astNodes )
    {
        if ( auto *fn = dynamic_cast<FunctionAST *> ( node.get () ) )
        {
            if ( !fn->registerSignature ( symTab ) )
                semanticErrors++;
        }
    }

    // --- SEMANTIC PASS 2: Full type-check all nodes ---
    // FunctionRegistry is fully populated now, so CallAST::typeCheck can
    // resolve any callee regardless of declaration order.
    if ( semanticErrors == 0 )
    {
        for ( const auto &node : astNodes )
        {
            if ( !node->typeCheck ( symTab ) )
                semanticErrors++;
        }
    }

    if ( semanticErrors > 0 )
    {
        std::cerr << "\nCompilation aborted due to " << semanticErrors << " semantic error(s).\n";
        return 1;
    }

    if ( dumpAST )
    {
        std::cout << "\n============================\n";
        std::cout << "       ABSTRACT SYNTAX TREE   \n";
        std::cout << "\n============================\n";

        for ( const auto &node : astNodes )
        {
            node->dump ( 0 );
        }

        std::cout << "\n============================\n";
    }

    std::cout << "Rivet Compiler initialized.\n";

    auto *Int32Ty = llvm::Type::getInt32Ty ( *CompilerState.TheContext );
    auto *EntryFnTy = llvm::FunctionType::get ( Int32Ty, false );
    auto *EntryFn = llvm::Function::Create ( EntryFnTy, llvm::Function::ExternalLinkage, "__rivet_entry",
                                             CompilerState.TheModule.get () );
    auto *EntryBB = llvm::BasicBlock::Create ( *CompilerState.TheContext, "entry", EntryFn );
    CompilerState.Builder->SetInsertPoint ( EntryBB );

    llvm::Value *lastVal = llvm::ConstantInt::get ( Int32Ty, 0, true );

    // --- CODEGEN PASS 1: Create all function prototypes ---
    // Ensures that CallAST::codegen() can always find its callee via
    // TheModule->getFunction(), regardless of source declaration order.
    for ( const auto &node : astNodes )
    {
        if ( auto *fn = dynamic_cast<FunctionAST *> ( node.get () ) )
        {
            if ( !fn->createPrototype () )
            {
                std::cerr << "Error: Failed to create prototype for a function.\n";
                return 1;
            }
        }
    }

    // --- CODEGEN PASS 2: Full code generation ---
    for ( const auto &node : astNodes )
    {
        if ( llvm::Value *val = node->codegen () )
        {
            lastVal = val;
        }
        else
        {
            std::cerr << "Error: Code generation failed for an AST node.\n";
            return 1;
        }
    }

    // --- Wire __rivet_entry → main() ---
    // FunctionAST::codegen() creates each user function as a separate LLVM
    // function and moves the builder's insert point into it.  After the loop
    // above, the builder sits inside the last user function (e.g. "main"),
    // leaving __rivet_entry's entry block with NO terminator and NO call.
    CompilerState.Builder->SetInsertPoint ( EntryBB );

    llvm::Function *mainFn = CompilerState.TheModule->getFunction ( "main" );
    if ( mainFn )
    {
        llvm::Value *result = CompilerState.Builder->CreateCall ( mainFn );
        CompilerState.Builder->CreateRet ( result );
    }
    else
    {
        // No main() found — return 0 as a fallback.
        CompilerState.Builder->CreateRet ( llvm::ConstantInt::get ( Int32Ty, 0, true ) );
    }

    std::cout << "============================\n";
    std::cout << "           LLVM IR          \n";
    std::cout << "============================\n";
    CompilerState.TheModule->print ( llvm::outs (), nullptr );
    std::cout << "============================\n";

    // --- CODEGEN PASS 3: Flexible Object Code Emission ---
    CompilerState.TheModule->setTargetTriple ( targetTriple );

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget ( targetTriple, Error );

    if ( !Target )
    {
        std::cerr << "Error: Could not allocate target: " << Error << "\n";
        return 1;
    }

    auto Features = "";
    llvm::TargetOptions opt;
    opt.ExceptionModel = llvm::ExceptionHandling::None; // Rivet has no exceptions — suppress .ARM.exidx
    auto RM = std::optional<llvm::Reloc::Model> ();
    auto TheTargetMachine = Target->createTargetMachine ( targetTriple, cpu, Features, opt, RM );

    CompilerState.TheModule->setDataLayout ( TheTargetMachine->createDataLayout () );

    // Strip "uwtable" and add "nounwind" to all functions.
    // LLVM's ARM backend emits .ARM.exidx (exception index) entries for every
    // function that might unwind. Since Rivet has no exceptions, we mark all
    // functions as nounwind and remove uwtable. This completely eliminates
    // .ARM.exidx from the output, avoiding the libgcc unwinder dependency chain
    // (which would drag in memcpy, abort, and all of newlib).
    for ( auto &F : *CompilerState.TheModule )
    {
        F.removeFnAttr ( llvm::Attribute::UWTable );
        F.addFnAttr ( llvm::Attribute::NoUnwind );
    }

    auto OutputFilename = "output.o";
    std::error_code EC;
    llvm::raw_fd_ostream dest ( OutputFilename, EC, llvm::sys::fs::OF_None );

    if ( EC )
    {
        std::cerr << "Could not open file: " << EC.message () << "\n";
        return 1;
    }

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CodeGenFileType::ObjectFile;
    if ( TheTargetMachine->addPassesToEmitFile ( pass, dest, nullptr, FileType ) )
    {
        std::cerr << "TargetMachine can't emit a file of this type\n";
        return 1;
    }

    pass.run ( *CompilerState.TheModule );
    dest.flush ();

    std::cout << "Successfully emitted object file to " << OutputFilename << ".\n";

    // =========================================================================
    // THE RIVET DRIVER: Flexible Bare-Metal Linking
    //
    // Activates for any target triple that contains "-none-" (the standard
    // bare-metal OS component in LLVM/GNU triples). WASM is excluded because
    // it uses its own module ABI — GNU linker scripts do not apply.
    //
    // All values flow from the user-supplied --target / --mcpu / --flash-* /
    // --ram-* flags. Nothing is hardcoded.
    // =========================================================================
    // Helper: parse a memory value string that may carry a K/M suffix or be hex.
    // e.g. "128K" -> 131072, "512K" -> 524288, "0x20000" -> 131072, "65536" -> 65536
    // This is needed because K/M suffixes are linker-script syntax, NOT assembler syntax.
    auto parseMemValue = [] ( const std::string &s ) -> uint64_t
    {
        if ( s.empty () )
            return 0;
        char suffix = s.back ();
        if ( suffix == 'K' || suffix == 'k' )
            return std::stoull ( s.substr ( 0, s.size () - 1 ), nullptr, 0 ) * 1024ULL;
        if ( suffix == 'M' || suffix == 'm' )
            return std::stoull ( s.substr ( 0, s.size () - 1 ), nullptr, 0 ) * 1024ULL * 1024ULL;
        return std::stoull ( s, nullptr, 0 ); // plain decimal or 0x hex
    };

    // Pre-compute the initial stack pointer (top of RAM) as a single hex constant.
    // This must be a plain number in the .word directive — the GNU assembler does
    // NOT understand the K/M suffixes that are valid in linker scripts.
    uint64_t ramOriginVal = parseMemValue ( ramOrigin );
    uint64_t ramSizeVal = parseMemValue ( ramSize );
    uint64_t stackTopVal = ramOriginVal + ramSizeVal;
    std::ostringstream stackTopStream;
    stackTopStream << "0x" << std::hex << std::uppercase << stackTopVal;
    std::string stackTop = stackTopStream.str ();

    bool isBareMetalTarget =
        targetTriple.find ( "-none-" ) != std::string::npos && targetTriple.find ( "wasm" ) == std::string::npos;

    if ( isBareMetalTarget )
    {
        std::cout << "Bare-metal target detected (" << targetTriple << " / " << cpu
                  << "). Generating hardware interface...\n";

        // --- Derive artifact filenames from triple + cpu ---
        // Unique per target so multiple builds never collide in the same dir.
        std::string ldFilename = "rivet_" + targetTriple + "_" + cpu + ".ld";
        std::string asmFilename = "rivet_" + targetTriple + "_" + cpu + "_startup.s";
        std::string asmObjFile = "rivet_" + targetTriple + "_" + cpu + "_startup.o";

        // --- Toolchain Prefix Derivation + Normalization ---
        // Default: the GNU cross-toolchain binary prefix mirrors the triple.
        // Normalization corrects architectures where the installed toolchain
        // uses a canonical prefix that does NOT match the LLVM triple literally.
        std::string toolchainPrefix = targetTriple + "-";

        if ( targetTriple.find ( "riscv" ) != std::string::npos )
        {
            // The GNU RISC-V multilib toolchain ships as riscv64-unknown-elf-{gcc,ld}
            // regardless of rv32/rv64 — it switches behaviour via -march at assemble time.
            toolchainPrefix = "riscv64-unknown-elf-";
        }
        // Future normalization entries go here, e.g.:
        // if (targetTriple.find("mips") != std::string::npos) { ... }

        // --- ISA detection ---
        // ARM and Thumb targets require the .thumb assembler directive and -mthumb flag.
        bool isThumb =
            targetTriple.find ( "arm" ) != std::string::npos || targetTriple.find ( "thumb" ) != std::string::npos;

        // ---- 1. Generate the Linker Script ----
        std::string linkerScript = "ENTRY(Reset_Handler)\n"
                                   "MEMORY {\n"
                                   "  FLASH (rx) : ORIGIN = " +
                                   flashOrigin + ", LENGTH = " + flashSize +
                                   "\n"
                                   "  RAM (rwx)  : ORIGIN = " +
                                   ramOrigin + ", LENGTH = " + ramSize +
                                   "\n"
                                   "}\n"
                                   "SECTIONS {\n"
                                   "  .text : {\n"
                                   "    KEEP(*(.isr_vector))  /* Vector table must be first */\n"
                                   "    *(.text*)\n"
                                   "    *(.rodata*)\n"
                                   "  } > FLASH\n"
                                   "  /* ARM exception index table.                                         */\n"
                                   "  /* __exidx_start/_end are required by libgcc's unwinder (unwind-arm) */\n"
                                   "  __exidx_start = .;\n"
                                   "  .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH\n"
                                   "  __exidx_end = .;\n"
                                   "  .data : { *(.data*) } > RAM AT > FLASH\n"
                                   "  .bss  : { *(.bss*) *(COMMON) } > RAM\n"
                                   "  /* Heap boundary symbols required by newlib's _sbrk (libnosys) */\n"
                                   "  end = .;\n"
                                   "  _end = .;\n"
                                   "  __end__ = .;\n"
                                   "}\n";

        std::error_code EC_LD;
        llvm::raw_fd_ostream ldDest ( ldFilename, EC_LD, llvm::sys::fs::OF_None );
        if ( EC_LD )
        {
            std::cerr << "Error: Could not write linker script: " << EC_LD.message () << "\n";
            return 1;
        }
        ldDest << linkerScript;
        ldDest.flush ();

        // ---- 2. Generate the Startup Assembly ----
        // ARM/Thumb: full vector table + Reset_Handler -> __rivet_entry
        // Other ISAs: minimal _start -> __rivet_entry (generic fallback)
        std::string startupAsm;
        if ( isThumb )
        {
            startupAsm = ".syntax unified\n"
                         ".cpu " +
                         cpu +
                         "\n"
                         ".thumb\n\n"
                         ".global vtable\n"
                         ".global Reset_Handler\n\n"
                         ".section .isr_vector,\"a\",%progbits\n"
                         "vtable:\n"
                         "    .word " +
                         stackTop + "  /* Initial SP: top of RAM (" + ramOrigin + " + " + ramSize +
                         ") */\n"
                         "    .word Reset_Handler                          /* Reset Vector */\n\n"
                         ".section .text.Reset_Handler\n"
                         ".type Reset_Handler, %function\n"
                         "Reset_Handler:\n"
                         "    bl __rivet_entry\n"
                         "hang:\n"
                         "    b hang\n"
                         /* Mark stack as non-executable; suppresses linker warning */
                         "\n.section .note.GNU-stack,\"\",%progbits\n";
        }
        else
        {
            // Generic bare-metal fallback (RISC-V, ARC, etc.)
            startupAsm = "/* Rivet startup for " + targetTriple + " / " + cpu +
                         " */\n"
                         ".global _start\n"
                         ".section .text\n"
                         "_start:\n"
                         "    call __rivet_entry\n"
                         "hang:\n"
                         "    j hang\n";
        }

        std::error_code EC_ASM;
        llvm::raw_fd_ostream asmDest ( asmFilename, EC_ASM, llvm::sys::fs::OF_None );
        if ( EC_ASM )
        {
            std::cerr << "Error: Could not write startup assembly: " << EC_ASM.message () << "\n";
            return 1;
        }
        asmDest << startupAsm;
        asmDest.flush ();

        // ---- 3. Assemble the startup file ----
        std::cout << "Assembling startup code with " << toolchainPrefix << "gcc...\n";
        std::string assembleCmd = toolchainPrefix + "gcc -c -mcpu=" + cpu + ( isThumb ? " -mthumb" : "" ) + " " +
                                  asmFilename + " -o " + asmObjFile;
        int asmResult = system ( assembleCmd.c_str () );
        if ( asmResult != 0 )
        {
            std::cerr << "Error: Failed to assemble startup code.\n"
                      << "  Command was: " << assembleCmd << "\n"
                      << "  Ensure " << toolchainPrefix << "gcc is installed.\n";
            return 1;
        }

        // ---- 4. Link everything into a firmware ELF ----
        // NOTE: system() calls are a temporary scaffold.
        // These will be replaced with lld (LLVM's built-in linker) in a future step
        // to remove the GNU toolchain runtime dependency entirely.
        //
        // We use the raw linker directly since we have eliminated .ARM.exidx
        // exception tables from the output (via ExceptionModel::None + stripping
        // uwtable attributes). This means libgcc is not needed and we avoid the
        // entire memcpy/abort/newlib dependency chain.
        std::cout << "Linking with " << toolchainPrefix << "ld...\n";
        std::string linkCmd =
            toolchainPrefix + "ld" + " -T " + ldFilename + " " + asmObjFile + " output.o -o firmware.elf";
        int linkResult = system ( linkCmd.c_str () );
        if ( linkResult != 0 )
        {
            std::cerr << "Error: Linker failed.\n"
                      << "  Command was: " << linkCmd << "\n";
            return 1;
        }

        std::cout << "SUCCESS: Generated bare-metal executable -> firmware.elf\n";
        std::cout << "  Target : " << targetTriple << "\n";
        std::cout << "  CPU    : " << cpu << "\n";
        std::cout << "  Flash  : " << flashOrigin << " / " << flashSize << "\n";
        std::cout << "  RAM    : " << ramOrigin << " / " << ramSize << "\n";
    }

    return 0;
}

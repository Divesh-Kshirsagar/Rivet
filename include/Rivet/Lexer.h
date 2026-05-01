#ifndef RIVET_LEXER_H
#define RIVET_LEXER_H

#include <string>
#include <fstream>

namespace Rivet
{

    enum Token
    {
        tok_eof = -1,

        // Dynamic Tokens
        tok_identifier = -2,
        tok_number = -3,
        tok_string = -4,

        // Memory & Types
        tok_int = -10,
        tok_void = -11,
        tok_ref = -12,
        tok_address_of = -13,
        tok_deref = -14,
        tok_optref = -15,

        // Logical & Bitwise
        tok_and = -20,
        tok_or = -21,
        tok_not = -22,
        tok_lsft = -23,
        tok_rsft = -24,
        tok_eq = -25,  // '=='
        tok_neq = -26, // '!='

        // Control Flow
        tok_if = -30,
        tok_else = -31,
        tok_while = -32,
        tok_for = -33,
        tok_fun = -34,
        tok_return = -35,
        tok_import = -36,
        tok_in = -37,
        tok_step = -38
    };

    class Lexer
    {
    public:
        Lexer(const std::string &filename);
        ~Lexer();

        int getNextToken();

        std::string IdentifierStr;

        int NumVal;

        int CurrentLine;
        int CurrentColumn;

    private:
        std::ifstream inputFile;
        char LastChar;

        int advanceChar();

        int gettok();
    };

}

#endif

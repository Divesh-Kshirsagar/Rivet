/**
 * @file Lexer.h
 * @brief Defines the Lexer and Token types for the Rivet compiler.
 * 
 * The lexer is responsible for performing lexical analysis on a given Rivet
 * source file, breaking down the raw text characters into a stream of tokens
 * that are consumed by the parser.
 */
#ifndef RIVET_LEXER_H
#define RIVET_LEXER_H

#include <string>
#include <fstream>

namespace Rivet
{
    /**
     * @brief Represents the possible token types in the Rivet language.
     * 
     * Positive token values correspond directly to their single-character ASCII
     * values (e.g., '+', '-'). Negative values represent multi-character constructs
     * such as keywords, literals, and identifiers.
     */
    enum Token
    {
        tok_eof = -1,           ///< End of file marker
        
        // Dynamic Tokens
        tok_identifier = -2,    ///< Represents a user-defined identifier (variable, function name)
        tok_number = -3,        ///< Represents an integer numeric literal
        tok_string_literal = -4,///< Represents a double-quoted string literal

        // Memory & Types
        tok_int = -10,          ///< Keyword 'int'
        tok_void = -11,         ///< Keyword 'void'
        tok_string = -12,       ///< Keyword 'str' (used for strings)
        tok_ref = -13,          ///< Keyword 'ref'
        tok_address_of = -14,   ///< Keyword 'address_of'
        tok_deref = -15,        ///< Keyword 'deref'
        tok_optref = -16,       ///< Keyword 'optref'

        // Logical & Bitwise
        tok_and = -20,          ///< Keyword 'and'
        tok_or = -21,           ///< Keyword 'or'
        tok_not = -22,          ///< Keyword 'not'
        tok_lsft = -23,         ///< Keyword 'lsft' (left shift)
        tok_rsft = -24,         ///< Keyword 'rsft' (right shift)
        tok_eq = -25,           ///< Operator '=='
        tok_neq = -26,          ///< Operator '!='

        // Control Flow
        tok_if = -30,           ///< Keyword 'if'
        tok_else = -31,         ///< Keyword 'else'
        tok_while = -32,        ///< Keyword 'while'
        tok_for = -33,          ///< Keyword 'for'
        tok_fun = -34,          ///< Keyword 'fun'
        tok_return = -35,       ///< Keyword 'return'
        tok_import = -36,       ///< Keyword 'import'
        tok_in = -37,           ///< Keyword 'in'
        tok_step = -38,         ///< Keyword 'step'
        tok_to = -39            ///< Keyword 'to'
    };

    /**
     * @class Lexer
     * @brief Performs lexical analysis on Rivet source code.
     * 
     * The `Lexer` scans through the contents of an input file character by character,
     * grouping them into meaningful definitions (`Token`). It also keeps track of
     * line/column data to assist with error reporting.
     */
    class Lexer
    {
    public:
        /**
         * @brief Constructs a Lexer attached to a specific input file.
         * @param filename Path to the source file to open and lex.
         */
        Lexer(const std::string &filename);
        
        /**
         * @brief Destructor that closes the held file stream.
         */
        ~Lexer();

        /**
         * @brief Requests the next token from the input stream.
         * @return The next `Token` value, or the ASCII character code if it's a single-char token.
         */
        int getNextToken();

        std::string IdentifierStr; ///< Contains the string value if the current token is `tok_identifier`.
        std::string StringVal;     ///< Contains the string value if the current token is `tok_string_literal`.
        int NumVal;                ///< Contains the numeric value if the current token is `tok_number`.

        int CurrentLine;           ///< The current line number in the source file being lexed (1-based).
        int CurrentColumn;         ///< The current column number on the active line.

    private:
        std::ifstream inputFile;   ///< Stream holding the file data.
        char LastChar;             ///< The most recently consumed character from the file.

        /**
         * @brief Reads the next character from the file schema and manages line/column offsets.
         * @return The scanned character or EOF if the end is reached.
         */
        int advanceChar();

        /**
         * @brief The internal implementation that scans ahead to produce the next token.
         * @return The parsed token ID.
         */
        int gettok();
    };

}

#endif

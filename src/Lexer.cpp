#include "Rivet/Lexer.h"
#include <iostream>
#include <cctype>

namespace Rivet
{
    Lexer::Lexer(const std::string &filename) : CurrentLine(1), CurrentColumn(0), LastChar(' ')
    {
        inputFile.open(filename);
        if (!inputFile.is_open())
        {
            std::cerr << "Fatal Error: Could not open file " << filename << std::endl;
            exit(1);
        }
    }

    Lexer::~Lexer()
    {
        if (inputFile.is_open())
        {
            inputFile.close();
        }
    }

    int Lexer::advanceChar()
    {
        if (inputFile.get(LastChar))
        {
            if (LastChar == '\n')
            {
                CurrentLine++;
                CurrentColumn = 0;
            }
            else
            {
                CurrentColumn++;
            }
            return LastChar;
        }
        else
        {
            LastChar = EOF;
            return EOF;
        }
    }

    int Lexer::gettok()
    {
        while (isspace(LastChar))
            advanceChar();

        if (LastChar == '/' && inputFile.peek() == '/')
        {
            advanceChar();
            while (LastChar != '\n' && LastChar != '\r' && !inputFile.eof())
            {
                advanceChar();
            }
            if (!inputFile.eof())
            {
                return gettok();
            }
        }

        // Identifiers and keywords
        if (isalpha(LastChar) || LastChar == '_')
        {
            IdentifierStr = LastChar;
            while (isalnum(advanceChar()) || LastChar == '_')
                IdentifierStr += LastChar;

            // Types and memory
            if (IdentifierStr == "int")
                return tok_int;
            if (IdentifierStr == "str")
                return tok_string;
            if (IdentifierStr == "void")
                return tok_void;
            if (IdentifierStr == "ref")
                return tok_ref;
            if (IdentifierStr == "address_of")
                return tok_address_of;
            if (IdentifierStr == "deref")
                return tok_deref;
            if (IdentifierStr == "optref")
                return tok_optref;

            // Control flow
            if (IdentifierStr == "if")
                return tok_if;
            if (IdentifierStr == "else")
                return tok_else;
            if (IdentifierStr == "while")
                return tok_while;
            if (IdentifierStr == "for")
                return tok_for;
            if (IdentifierStr == "in")
                return tok_in;
            if (IdentifierStr == "step")
                return tok_step;
            if (IdentifierStr == "fun")
                return tok_fun;
            if (IdentifierStr == "return")
                return tok_return;
            if (IdentifierStr == "import")
                return tok_import;

            // Logical and bitwise
            if (IdentifierStr == "and")
                return tok_and;
            if (IdentifierStr == "or")
                return tok_or;
            if (IdentifierStr == "not")
                return tok_not;
            if (IdentifierStr == "lsft")
                return tok_lsft;
            if (IdentifierStr == "rsft")
                return tok_rsft;

            return tok_identifier;
        }

        // Number: [0-9]+
        if (isdigit(LastChar))
        {
            std::string NumStr;
            do
            {
                NumStr += LastChar;
                advanceChar();
            } while (isdigit(LastChar));

            NumVal = std::stoi(NumStr);
            return tok_number;
        }

        // Operators and punctuation
        if (LastChar == '=')
        {
            if (inputFile.peek() == '=')
            {
                advanceChar();
                advanceChar();
                return tok_eq;
            }
        }

        if (LastChar == '!')
        {
            if (inputFile.peek() == '=')
            {
                advanceChar();
                advanceChar();
                return tok_neq;
            }
        }

        if (LastChar == EOF)
            return tok_eof;

        // Otherwise, just return the character as its ascii value
        int ThisChar = LastChar;
        advanceChar();
        return ThisChar;
    }

    int Lexer::getNextToken()
    {
        return gettok();
    }
}

/**
 * @file Parser.h
 * @brief Defines the Parser class for analyzing the token stream.
 * 
 * The Parser class uses recursive descent to process the token stream supplied
 * by the Lexer and constructs an Abstract Syntax Tree (AST) representing the program.
 */
#ifndef RIVET_PARSER_H
#define RIVET_PARSER_H

#include "Lexer.h"
#include "AST.h"
#include <memory>
#include <vector>
#include <set>

namespace Rivet
{
    /**
     * @class Parser
     * @brief Parses a stream of tokens into an Abstract Syntax Tree (AST).
     * 
     * Uses recursive descent parsing techniques to evaluate tokens sequence
     * rules, ensuring grammatical correctness based on the EBNF definitions.
     */
    class Parser
    {
    public:
        /**
         * @brief Constructs a Parser using a specific Lexer instance.
         * @param lexer The lexical analyzer configured with the input source stream.
         */
        Parser(Lexer& lexer);

        /**
         * @brief Parses the entire file bound to the Lexer.
         * @return A vector of unique pointers to the parsed root AST nodes (usually statements/declarations).
         */
        std::vector<std::unique_ptr<ASTNode>> ParseFile();

        int ErrorCount = 0; ///< Counter tracking how many syntax errors were encountered.
        inline static std::set<std::string> AlreadyImported; ///< Tracks imported modules to prevent cyclical/duplicate imports.

    private:
        Lexer& lexer;      ///< Reference to the injected Lexer.
        int CurTok;        ///< ID of the current token being processed.

        /**
         * @brief Prompts the Lexer for the next token and stores it in CurTok.
         * @return The integer ID of the consumed token.
         */
        int getNextToken();

        /**
         * @brief Logs a general syntax error and increments the ErrorCount.
         * @param Str The error message string.
         * @return Always returns nullptr so error propagation can occur smoothly.
         */
        std::unique_ptr<ASTNode> LogError(const char *Str);

        /**
         * @brief Logs an error based on a missing expected structural token.
         * @param expected The expected character or token name.
         * @param context The surrounding context where the failure occurred.
         * @return Always returns nullptr.
         */
        std::unique_ptr<ASTNode> LogErrorExpected(const char* expected, const char* context);

        // Expression parsing (handling operator precedence)
        
        /**
         * @brief Entry point for parsing basic and complex expressions.
         * @return The ASTNode representing the evaluated expression map.
         */
        std::unique_ptr<ASTNode> ParseExpression();

        /**
         * @brief Evaluates the Right Hand Side of a Binary Operator expression based on Operator-Precedence.
         * @param ExprPrec The minimum precedence necessary to continue absorbing operators.
         * @param LHS The previously evaluated Left Hand Side of the binary expression.
         * @return The combined tree representation.
         */
        std::unique_ptr<ASTNode> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ASTNode> LHS);

        /**
         * @brief Parses unary expressions (e.g. `!`, `-`, `address_of`).
         * @return The parsed Unary AST node or an underlying primitive expression.
         */
        std::unique_ptr<ASTNode> ParseUnaryExpr();
        
        // Parsing functions each for a major ebnf rule

        /**
         * @brief Parses a literal integer token.
         * @return A NumberAST node containing the parsed value.
         */
        std::unique_ptr<ASTNode> ParseNumberExpr();

        /**
         * @brief Parses a string literal token.
         * @return A StringLiteralAST node containing the text.
         */
        std::unique_ptr<ASTNode> ParseStringLiteralExpr();

        /**
         * @brief Parses a grouped parenthesis expression to override normal operator precedence.
         * @return The ASTNode evaluated within the parentheses.
         */
        std::unique_ptr<ASTNode> ParseParenExpr();

        /**
         * @brief Parses variables, array element accesses, and function calls.
         * @return The ASTNode representing the identified construct.
         */
        std::unique_ptr<ASTNode> ParseIdentifierExpr();
        
        // statements and control flow

        /**
         * @brief Parses a standalone generic statement type.
         * @return The root ASTNode of the statement.
         */
        std::unique_ptr<ASTNode> ParseStatement();

        /**
         * @brief Parses `if` and `if-else` control structures.
         * @return An IfAST node describing the conditional branches.
         */
        std::unique_ptr<ASTNode> ParseIfStatement();

        /**
         * @brief Parses `while()` iteration structures.
         * @return A WhileAST node describing the condition and body.
         */
        std::unique_ptr<ASTNode> ParseWhileStatement();

        /**
         * @brief Parses `for` iteration structures for ranges and arrays.
         * @return A ForAST node for the loop sequence.
         */
        std::unique_ptr<ASTNode> ParseForStatement();
        std::unique_ptr<ASTNode> ParseFunctionStatement();
        std::unique_ptr<ASTNode> ParseReturnStatement();

        /**
         * @brief Parses a block of statements enclosed in `{}`.
         * @return A BlockAST node accumulating all internal statements.
         */
        std::unique_ptr<ASTNode> ParseBlock();

        /**
         * @brief Parses variable declarations (e.g. `int x = 5;` or `int[5] arr;`).
         * @return A VariableDeclAST node recording type, name, and initial value.
         */
        std::unique_ptr<ASTNode> ParseVariableDeclaration();

        /**
         * @brief Parses module import statements, bringing in external files.
         * @return An ImportAST containing the nodes resolved from the module.
         */
        std::unique_ptr<ASTNode> ParseImport();
        
        /**
         * @brief Helper that maps token IDs to operator precedence rankings.
         * @return Precedence logic tier (higher value = tighter binding).
         */
        int GetTokPrecedence();
    };
}

#endif 

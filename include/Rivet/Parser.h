#ifndef RIVET_PARSER_H
#define RIVET_PARSER_H

#include "Lexer.h"
#include "AST.h"
#include <memory>
#include <vector>
#include <set>

namespace Rivet
{
    class Parser
    {
    public:
        Parser(Lexer& lexer);
        std::vector<std::unique_ptr<ASTNode>> ParseFile();
        int ErrorCount = 0;
        inline static std::set<std::string> AlreadyImported;

    private:
        Lexer& lexer;
        int CurTok;

        int getNextToken();

        std::unique_ptr<ASTNode> LogError(const char *Str);
        std::unique_ptr<ASTNode> LogErrorExpected(const char* expected, const char* context);

        
        // Expression parsing (handling operator precedence)
        std::unique_ptr<ASTNode> ParseExpression();
        std::unique_ptr<ASTNode> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ASTNode> LHS);
        std::unique_ptr<ASTNode> ParseUnaryExpr();
        
        // Parsing functions each for a major ebnf rule
        std::unique_ptr<ASTNode> ParseNumberExpr();
        std::unique_ptr<ASTNode> ParseStringLiteralExpr();        
        std::unique_ptr<ASTNode> ParseParenExpr();
        std::unique_ptr<ASTNode> ParseIdentifierExpr();
        
        // statements and control flow
        std::unique_ptr<ASTNode> ParseStatement(); // A helper block parser for '{ statements }'
        std::unique_ptr<ASTNode> ParseIfStatement();
        std::unique_ptr<ASTNode> ParseWhileStatement();
        std::unique_ptr<ASTNode> ParseForStatement();
        std::unique_ptr<ASTNode> ParseBlock();
        std::unique_ptr<ASTNode> ParseVariableDeclaration();
        std::unique_ptr<ASTNode> ParseImport();
        
        int GetTokPrecedence();
    };
}

#endif 
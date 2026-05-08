#include "Rivet/Parser.h"
#include <iostream>
#include <filesystem>

namespace Rivet
{
    Parser::Parser(Lexer &lexer) : lexer(lexer)
    {
        getNextToken();
    }

    int Parser::getNextToken()
    {
        CurTok = lexer.getNextToken();
        return CurTok;
    }

    std::unique_ptr<ASTNode> Parser::LogError(const char *str)
    {
        ErrorCount++;
        std::cerr << "\n[Compile Error] -> " << str << "\n";
        std::cerr << "  --> At Line " << lexer.CurrentLine << ", Column " << lexer.CurrentColumn << "\n\n";
        return nullptr;
    }

    std::unique_ptr<ASTNode> Parser::LogErrorExpected(const char *expected, const char *context)
    {
        std::string msg = std::string("Expected '") + expected + "' " + context;
        return LogError(msg.c_str());
    }

    std::unique_ptr<ASTNode> Parser::ParseNumberExpr()
    {
        auto result = std::make_unique<NumberAST>(lexer.NumVal);
        getNextToken();
        return result;
    }

    std::unique_ptr<ASTNode> Parser::ParseIdentifierExpr()
    {
        std::string idName = lexer.IdentifierStr;

        getNextToken();

        if (CurTok != '(')
            return std::make_unique<VariableAST>(idName);

        getNextToken();

        std::vector<std::unique_ptr<ASTNode>> args;
        if (CurTok != ')')
        {
            while (true)
            {
                if (auto arg = ParseExpression())
                {
                    args.push_back(std::move(arg));
                }
                else
                {
                    return nullptr;
                }

                if (CurTok == ')')
                    break;

                if (CurTok != ',')
                    return LogErrorExpected("',' or ')'", "in argument list");

                getNextToken();
            }
        }

        getNextToken();

        return std::make_unique<CallAST>(idName, std::move(args));
    }

    std::unique_ptr<ASTNode> Parser::ParseIfStatement()
    {
        getNextToken();

        if (CurTok != '(')
            return LogErrorExpected("'('", "after 'if'");
        getNextToken();

        auto Cond = ParseExpression();
        if (!Cond)
            return nullptr;

        if (CurTok != ')')
            return LogErrorExpected("')'", "after condition");
        getNextToken();
        auto Then = ParseBlock();
        if (!Then)
            return nullptr;
        std::unique_ptr<ASTNode> Else = nullptr;
        if (CurTok == tok_else)
        {
            getNextToken();
            Else = ParseBlock();
            if (!Else)
                return nullptr;
        }
        return std::make_unique<IfAST>(std::move(Cond), std::move(Then), std::move(Else));
    }

    std::unique_ptr<ASTNode> Parser::ParseWhileStatement()
    {
        getNextToken();

        if (CurTok != '(')
            return LogErrorExpected("'('", "after 'while'");
        getNextToken();

        auto Cond = ParseExpression();
        if (!Cond)
            return nullptr;

        if (CurTok != ')')
            return LogErrorExpected("')'", "after condition");
        getNextToken();
        auto Body = ParseBlock();
        if (!Body)
            return nullptr;

        return std::make_unique<WhileAST>(std::move(Cond), std::move(Body));
    }

    // Dummy implementations for now, to be filled in later
    // TODO: Replace temroary placeholder with actual parsing logic
    std::unique_ptr<ASTNode> Parser::ParseBlock()
    {
        if (CurTok != '{')
            return LogErrorExpected("{", "to start block");
        getNextToken();

        std::vector<std::unique_ptr<ASTNode>> Statements;

        while (CurTok != '}' && CurTok != tok_eof)
        {
            auto Stmt = ParseStatement();
            if (Stmt)
            {
                Statements.push_back(std::move(Stmt));
            }
            else
            {
                getNextToken();
            }
        }

        if (CurTok != '}')
            return LogErrorExpected("}", "to end block");
        getNextToken();

        return std::make_unique<BlockAST>(std::move(Statements));
    }

    std::unique_ptr<ASTNode> Parser::ParseExpression()
    {
        auto LHS = ParseUnaryExpr();
        if (!LHS)
            return nullptr;
        return ParseBinOpRHS(0, std::move(LHS));
    }

    std::vector<std::unique_ptr<ASTNode>> Parser::ParseFile()
    {
        std::vector<std::unique_ptr<ASTNode>> Nodes;
        while (CurTok != tok_eof)
        {
            if (auto stmt = ParseStatement())
            {
                Nodes.push_back(std::move(stmt));
            }
            else
            {
                getNextToken();
            }
        }
        return Nodes;
    }

    std::unique_ptr<ASTNode> Parser::ParseStatement()
    {
        if (CurTok == tok_import)
            return ParseImport();
        if (CurTok == tok_int)
            return ParseVariableDeclaration();
        if (CurTok == tok_if)
            return ParseIfStatement();
        if (CurTok == tok_while)
            return ParseWhileStatement();
        if (CurTok == tok_for)
            return ParseForStatement();
        if (CurTok == '{')
            return ParseBlock();
        if (CurTok == ';')
        {
            getNextToken();
            return std::make_unique<BlockAST>(std::vector<std::unique_ptr<ASTNode>>{});
        }

        auto Expr = ParseExpression();
        if (!Expr)
            return nullptr;
        if (CurTok != ';')
            return LogErrorExpected("';'", "after expression");
        getNextToken();
        return Expr;
    }

    std::unique_ptr<ASTNode> Parser::ParseParenExpr()
    {
        if (CurTok != '(')
            return LogErrorExpected("'('", "to start expression");
        getNextToken();
        auto V = ParseExpression();
        if (!V)
            return nullptr;
        if (CurTok != ')')
            return LogErrorExpected("')'", "to end expression");
        getNextToken();
        return V;
    }

    int Parser::GetTokPrecedence()
    {
        if (CurTok < 0)
        {
            switch (CurTok)
            {
            case tok_eq:
            case tok_neq:
                return 10;
            case tok_and:
                return 5;
            case tok_or:
                return 4;
            case tok_lsft:
            case tok_rsft:
                return 20;
            default:
                return -1;
            }
        }
        else
        {
            switch (CurTok)
            {
            case '=':
                return 2;
            case '<':
            case '>':
                return 15;
            case '+':
            case '-':
                return 20;
            case '*':
            case '/':
                return 40;
            default:
                return -1;
            }
        }
    }

    std::unique_ptr<ASTNode> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ASTNode> LHS)
    {
        while (true)
        {
            int TokPrec = GetTokPrecedence();

            if (TokPrec < ExprPrec)
                return LHS;

            int BinOp = CurTok;
            getNextToken();

            auto RHS = ParseUnaryExpr();
            if (!RHS)
                return nullptr;

            int NextPrec = GetTokPrecedence();
            if (TokPrec < NextPrec)
            {
                RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
                if (!RHS)
                    return nullptr;
            }

            LHS = std::make_unique<BinaryOpAST>(BinOp, std::move(LHS), std::move(RHS));
        }
    }

    std::unique_ptr<ASTNode> Parser::ParseUnaryExpr()
    {
        if (CurTok == tok_not || CurTok == '-') 
        {
            int Op = CurTok;
            getNextToken();
            auto Operand = ParseUnaryExpr(); // Recursively handles --x, -(a+b)
            if (!Operand)
                return nullptr;
            return std::make_unique<UnaryOpAST>(Op, std::move(Operand));
        }
        if (CurTok == tok_identifier)
            return ParseIdentifierExpr();
        if (CurTok == '(')
            return ParseParenExpr();
        if (CurTok == tok_number)
            return ParseNumberExpr();
        // TODO: Implement optref 
        if (CurTok == tok_address_of)
        {
            getNextToken();
            if (CurTok != tok_identifier)
                return LogErrorExpected("identifier", "after address-of operator");
            std::string VarName = lexer.IdentifierStr;
            getNextToken();
            return std::make_unique<AddressOfAST>(std::move(VarName));
        }
        if (CurTok == tok_deref)
        {
            getNextToken();
            auto Operand = ParseUnaryExpr();
            if (!Operand)
                return nullptr;
            return std::make_unique<DerefAST>(std::move(Operand));
        }
        
        return LogError("Unknown token when expecting unary expression");
    }

    std::unique_ptr<ASTNode> Parser::ParseVariableDeclaration()
    {
        if (CurTok != tok_int)
            return LogErrorExpected("int", "to start variable declaration");
        getNextToken();

        bool isRef = false;
        if (CurTok == tok_ref)
        {
            isRef = true;
            getNextToken();
        }

        if (CurTok != tok_identifier)
            return LogErrorExpected("identifier", "after type in variable declaration");
        std::string varName = lexer.IdentifierStr;
        getNextToken();

        std::unique_ptr<ASTNode> initVal = nullptr;
        if (CurTok == '=')
        {
            getNextToken();
            initVal = ParseExpression();
            if (!initVal)
                return nullptr;
        }
        if (CurTok != ';')
            return LogErrorExpected("';'", "after variable declaration");
        getNextToken();
        return std::make_unique<VariableDeclAST>("int", varName, isRef, std::move(initVal));
    }

    std::unique_ptr<ASTNode> Parser::ParseImport()
    {
        if (CurTok != tok_import)
            return LogErrorExpected("import", "to start import statement");

        getNextToken();
        if (CurTok != tok_identifier)
            return LogErrorExpected("module name", "after 'import'");

        std::string moduleName = lexer.IdentifierStr;
        getNextToken();
        if (CurTok != ';')
            return LogErrorExpected("';'", "after import statement");
        getNextToken();
        // To avoid duplicate imports
        if (AlreadyImported.count(moduleName))
        {
            return std::make_unique<ImportAST>(moduleName, std::vector<std::unique_ptr<ASTNode>>{});
        }
        AlreadyImported.insert(moduleName);

        std::string importPath = "lib/" + moduleName + ".rvt"; 
        
        // This fallback is done so that the compiler could pick up the file whether rivet is executed from the repo root or from the build directory.
        if (!std::filesystem::exists(importPath))
        {
            const std::string fallbackPath = "../lib/" + moduleName + ".rvt";
            if (std::filesystem::exists(fallbackPath))
            {
                importPath = fallbackPath;
            }
        }

        Lexer importLexer(importPath);
        Parser importParser(importLexer);

        std::cout << "Injecting source from module:" << moduleName << std::endl;
        auto importedAST = importParser.ParseFile();

        if (importParser.ErrorCount > 0)
        {
            return LogError("Failed to import module.");
        }
        return std::make_unique<ImportAST>(moduleName, std::move(importedAST));
    }

    std::unique_ptr<ASTNode> Parser::ParseForStatement()
    {
        // Consume 'for'
        getNextToken();

        // Consume '('
        if (CurTok != '(')
            return LogErrorExpected("(", "after 'for'");
        getNextToken();
    
        // Iterator variable name
        if (CurTok != tok_identifier)
            return LogErrorExpected("identifier", "after '('");
        
        std::string varName = lexer.IdentifierStr;
        getNextToken();
    
        // Consume 'in'
        if (CurTok != tok_in)
            return LogErrorExpected("'in'", "after iterator variable");
        getNextToken();
    
        // Start expression
        auto start = ParseExpression();
        if (!start) 
            return nullptr;
    
        // Consume 'to'
        if (CurTok != tok_to)
            return LogErrorExpected("'to'", "after start expression");
        getNextToken();
    
        // End expression
        auto end = ParseExpression();
        if (!end) 
            return nullptr;
    
        // Optional Step
        std::unique_ptr<ASTNode> step = nullptr;
        if (CurTok == tok_step)
        {
            getNextToken();
            step = ParseExpression();
            if (!step) 
                return nullptr;
        }

        // Consume ')'
        if (CurTok != ')')
            return LogErrorExpected(")", "at end of 'for'");
        
        getNextToken();
    
        // Loop Body
        auto body = ParseBlock();
        if (!body) 
            return nullptr;
    
        return std::make_unique<ForAST>(varName, std::move(start), std::move(end), std::move(step), std::move(body));
    }
}

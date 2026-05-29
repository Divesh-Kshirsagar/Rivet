/**
 * @file Parser.cpp
 * @brief Implementation of the Parser logic.
 */
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

    std::unique_ptr<ASTNode> Parser::ParseStringLiteralExpr()
    {
        // grab string value stored by the lexer
        auto result = std::make_unique<StringLiteralAST>(lexer.StringVal);
        getNextToken();
        return result;
    }

    std::unique_ptr<ASTNode> Parser::ParseIdentifierExpr()
    {
        std::string idName = lexer.IdentifierStr;

        getNextToken();

        // Compiler intrinsic: __volatile_store(address, value)
        if (idName == "__volatile_store")
        {
            if (CurTok != '(')
                return LogErrorExpected("'('", "after '__volatile_store'");
            getNextToken();

            auto addressExpr = ParseExpression();
            if (!addressExpr)
                return nullptr;

            if (CurTok != ',')
                return LogErrorExpected("','", "between __volatile_store arguments");
            getNextToken();

            auto valueExpr = ParseExpression();
            if (!valueExpr)
                return nullptr;

            if (CurTok != ')')
                return LogErrorExpected("')'", "after __volatile_store arguments");
            getNextToken();

            return std::make_unique<VolatileStoreAST>(std::move(addressExpr), std::move(valueExpr));
        }

        // function call f(a,b)
        if (CurTok == '(')
        {
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
            if (CurTok != ')')
                return LogErrorExpected("')'", "after argument list");
            getNextToken();
            return std::make_unique<CallAST>(idName, std::move(args));

        }
        // array indexing x[i]
        if (CurTok == '[')
        {
            getNextToken();
            auto indexExpr = ParseExpression();
            if (!indexExpr)
                return nullptr;

            if (CurTok != ']')
                return LogErrorExpected("']'", "after index expression");
            getNextToken();

            return std::make_unique<IndexAST>(idName, std::move(indexExpr));
        }

        // variable x
        return std::make_unique<VariableAST>(idName);
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

    // Parses a braced block and recovers from statement-level failures by
    // advancing one token to continue parsing the rest of the block.
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
        if (CurTok == tok_int || CurTok == tok_string)
            return ParseVariableDeclaration();
        if (CurTok == tok_if)
            return ParseIfStatement();
        if (CurTok == tok_while)
            return ParseWhileStatement();
        if (CurTok == tok_for)
            return ParseForStatement();
        if (CurTok == tok_fun)
            return ParseFunctionStatement();
        if (CurTok == tok_return)
            return ParseReturnStatement();
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
        if (CurTok == tok_string_literal)
            return ParseStringLiteralExpr();
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
        if (CurTok == tok_null)
        {
            getNextToken();
            return std::make_unique<NullLiteralAST>();
        }
        
        return LogError("Unknown token when expecting unary expression");
    }

    std::unique_ptr<ASTNode> Parser::ParseVariableDeclaration()
    {
        if (CurTok != tok_int && CurTok != tok_string)
            return LogErrorExpected("int or str", "to start variable declaration");

        const bool isStringType = (CurTok == tok_string);
        const std::string declaredType = isStringType ? "str" : "int";
        getNextToken();

        bool isRef = false;
        bool isOptRef = false;
        int arrayCapacity = 0; // default is 0 meaning it is not an array
        
        if (CurTok == tok_ref || CurTok == tok_optref)
        {
            if (isStringType)
                return LogError("String references are not supported yet.");
            // TODO: Investigate/resolve parsing edge where valid declarations
            // like `int ref p = ...;` and `int optref q = ...;` can still
            // surface "Expected 'identifier' after type in variable declaration"
            // in some frontend cases.
            if (CurTok == tok_optref)
                isOptRef = true;
            isRef = true;
            getNextToken();
        }

        if (CurTok == '[')
        {
            if (isStringType)
                return LogError("String arrays are not supported yet.");
            getNextToken();
            if (CurTok != tok_number)
                return LogErrorExpected("number", "after '[' in array declaration");
            arrayCapacity = lexer.NumVal;
            if (arrayCapacity <= 0)
                return LogError("Array capacity must be a positive integer (> 0).");
            getNextToken();
            if (CurTok != ']')
                return LogErrorExpected("']'", "after array capacity");
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
        return std::make_unique<VariableDeclAST>(declaredType, varName, isRef, isOptRef, std::move(initVal), arrayCapacity);
    }

    std::unique_ptr<ASTNode> Parser::ParseReturnStatement()
    {
        getNextToken(); // consume 'return'

        std::unique_ptr<ASTNode> RetVal = nullptr;
        if (CurTok != ';')
        {
            RetVal = ParseExpression();
            if (!RetVal)
                return nullptr;
        }

        if (CurTok != ';')
            return LogErrorExpected("';'", "after return statement");
        getNextToken();

        return std::make_unique<ReturnAST>(std::move(RetVal));
    }

    std::unique_ptr<ASTNode> Parser::ParseFunctionStatement()
    {
        getNextToken(); // consume 'fun'

        std::string returnType = "int";
        if (CurTok == tok_int || CurTok == tok_string || CurTok == tok_void)
        {
            if (CurTok == tok_int)
                returnType = "int";
            else if (CurTok == tok_string)
                returnType = "str";
            else
                returnType = "void";
            getNextToken();
        }

        if (CurTok != tok_identifier)
            return LogErrorExpected("function name", "after 'fun'");
        std::string fnName = lexer.IdentifierStr;
        getNextToken();

        if (CurTok != '(')
            return LogErrorExpected("'('", "after function name");
        getNextToken();

        std::vector<FunctionParam> params;
        if (CurTok != ')')
        {
            while (true)
            {
                if (CurTok != tok_int && CurTok != tok_string)
                    return LogErrorExpected("parameter type", "in function parameter list");

                std::string paramType = (CurTok == tok_int) ? "int" : "str";
                getNextToken();

                bool isRef = false;
                bool isOptRef = false;
                if (CurTok == tok_ref || CurTok == tok_optref)
                {
                    if (paramType == "str")
                        return LogError("String references are not supported yet.");
                    if (CurTok == tok_optref)
                        isOptRef = true;
                    isRef = true;
                    getNextToken();
                }

                if (CurTok != tok_identifier)
                    return LogErrorExpected("parameter name", "in function parameter list");
                std::string paramName = lexer.IdentifierStr;
                getNextToken();

                params.push_back(FunctionParam{paramType, paramName, isRef, isOptRef});

                if (CurTok == ')')
                    break;
                if (CurTok != ',')
                    return LogErrorExpected("',' or ')'", "in function parameter list");
                getNextToken();
            }
        }

        if (CurTok != ')')
            return LogErrorExpected("')'", "after function parameter list");
        getNextToken();

        auto body = ParseBlock();
        if (!body)
            return nullptr;

        return std::make_unique<FunctionAST>(fnName, returnType, std::move(params), std::move(body));
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

        std::unique_ptr<ASTNode> start = nullptr;
        if (CurTok == tok_identifier)
        {
            std::string firstIdent = lexer.IdentifierStr;
            getNextToken();

            // Array iteration form: for (item in arrayName) { ... }
            if (CurTok == ')')
            {
                getNextToken();

                auto body = ParseBlock();
                if (!body)
                    return nullptr;

                return std::make_unique<ForAST>(varName, firstIdent, std::move(body));
            }

            start = std::make_unique<VariableAST>(firstIdent);
            start = ParseBinOpRHS(0, std::move(start));
            if (!start)
                return nullptr;
        }
        else
        {
            start = ParseExpression();
            if (!start)
                return nullptr;
        }

        if (CurTok != tok_to)
            return LogErrorExpected("'to'", "after start expression");
        getNextToken();

        auto end = ParseExpression();
        if (!end)
            return nullptr;

        std::unique_ptr<ASTNode> step = nullptr;
        if (CurTok == tok_step)
        {
            getNextToken();
            step = ParseExpression();
            if (!step)
                return nullptr;
        }

        if (CurTok != ')')
            return LogErrorExpected(")", "at end of 'for'");

        getNextToken();

        auto body = ParseBlock();
        if (!body)
            return nullptr;

        return std::make_unique<ForAST>(varName, std::move(start), std::move(end), std::move(step), std::move(body));
    }
}

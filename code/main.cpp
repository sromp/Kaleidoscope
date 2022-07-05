#include <string>
#include <memory>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <cctype>

#define local_persist static
#define global_variable static
#define internal static

typedef float real32;
typedef double real64;

// ####################------------------------------------------------####################
//                                        L E X E R
// ####################------------------------------------------------####################

global_variable std::string IdentifierStr; // filled in if tok_identifier
global_variable real64 NumVal;            // filled in if tok_number

// NOTE(llvm): The lexer return tokens [0-255] if it is an unknown character,
// otherwise one of these for known things.
enum Token
{
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
};

// NOTE(srp): The lexer implementation
internal int
gettok()
{
    local_persist int LastChar = ' ';
    
    // Skip whitespace
    while (isspace(LastChar))
    {
        LastChar = getchar();
    }

    if (isalpha(LastChar)) // identifier: [a-zA-Z][a-zA-Z0-9]*
    {
        IdentifierStr = LastChar;

        while(isalnum((LastChar = getchar())))
        {
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def")
        {
            return tok_def;
        }

        if(IdentifierStr == "extern")
        {
            return tok_extern;
        }

        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.') // Number: [0-9.]+
    {
        std::string NumStr;
        do
        {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == ',');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#')
    {
        // comment until end of line
        do
        {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
        {
            return gettok();
        }
    }

    if (LastChar == EOF)
    {
        return tok_eof;
    }

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

// ####################------------------------------------------------####################
//                                     A S T
// ####################------------------------------------------------####################

// NOTE(llvm): Base class for all expression nodes
struct ExprAST
{
    virtual ~ExprAST() {}
};

// NOTE(llvm): Expression class for numeric literals
struct NumberExprAST : ExprAST
{
    real64 Val;

    NumberExprAST(real64 val) : Val(val) {}
};

// NOTE(llvm): Expression class for referencing a variable
struct VariableExprAST : ExprAST
{
    std::string Name;

    VariableExprAST(const std::string name) : Name(name) {}
};

// NOTE(llvm): Expression class for a binary operator
struct BinaryExprAST : ExprAST
{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
};

// NOTE(llvm): Expression class for function calls
struct CallExprAST : ExprAST
{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
        : Callee(callee), Args(std::move(args)) {}
};

// NOTE(llvm): This class represents the 'prototype' for a function,
// which captures its name and its arguments' names.
struct PrototypeAST
{
    std::string Name;
    std::vector<std::string> Args;

    PrototypeAST(const std::string &name, std::vector<std::string> args)
        : Name(name), Args(std::move(args)) {}
};

// NOTE(llvm): This class represents a function definition
struct FunctionAST
{
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

    FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body)
        : Proto(std::move(proto)), Body(std::move(body)) {}
};

// ####################------------------------------------------------####################
//                                     P A R S E R
// ####################------------------------------------------------####################

internal std::unique_ptr<ExprAST> ParseExpression();

// NOTE(llvm): Simple token buffer; it's the token the parser is looking at
global_variable int CurTok;

internal int
getNextToken()
{
    return CurTok = gettok();
}

// NOTE(llvm): This holds the precedence for each binary operator that is defined
global_variable std::map<char, int> BinopPrecedence;

// NOTE(llvm): Get the precedence of the pending binary operator token
internal int
GetTokPrecedence()
{
    if(!isascii(CurTok))
    {
        return -1;
    }

    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0)
    {
        return -1;
    }

    return TokPrec;
}

std::unique_ptr<ExprAST>
LogError(const char *Str)
{
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST>
LogErrorP(const char *Str)
{
    LogError(Str);
    return nullptr;
}


// numberexpr ::= number
// NOTE(srp): To be called on tok_number
internal std::unique_ptr<ExprAST>
ParseNumberExpr()
{
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // consume the number
    return std::move(Result);
}

// parenexpr ::= '(' expression ')'
// NOTE(srp): To be called on parenthesis operator (positive 0-255 token)
internal std::unique_ptr<ExprAST>
ParseParenExpr()
{
    getNextToken(); // eat '('
    auto V = ParseExpression(); // NOTE(srp): ParseExpression can call ParseParenExpr, so it
                                // can now handle recursive grammars. ParseParenExpr generates
                                // no new ExprAST, it only redirects and groups ones generated
                                // by subexpressions.
    if (!V)
    {
        return nullptr;
    }

    if (CurTok != ')')
    {
        return LogError("expected ')'");
    }

    getNextToken(); // eat ')'

    return V;
}

// identifierexpr
//      ::= identifier
//      ::= indentifier '(' expression* ')'
// NOTE(srp): To be called on tok_identifier
internal std::unique_ptr<ExprAST>
ParseIdentifierExpr()
{
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier

    // Variable reference
    if (CurTok != '(') // Look-ahead to determine if it's a variable reference or function call
    {
        return std::make_unique<VariableExprAST>(IdName);
    }

    // Function call
    getNextToken(); // eat '('
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')')
    {
        for (;;)
        {
            if (auto Arg = ParseExpression())
            {
                Args.push_back(std::move(Arg));
                /* Args.emplace_back(std::move(Arg)); */
            }
            else
            {
                return nullptr;
            }

            if (CurTok == ')')
            {
                break;
            }

            if (CurTok != ',')
            {
                return LogError("Expected ')' or ',' in argument list");
            }
            getNextToken();
        }
    }

    // eat ')'
    getNextToken();
    
    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// primary
//      ::= identifier
//      ::= numberexpr
//      ::= parenexpr
// NOTE(srp): Helper function as entry point for primary expression parsing
internal std::unique_ptr<ExprAST> 
ParsePrimary()
{
    switch (CurTok)
    {
        default:
            return LogError("Unknown token when expecting an expression");

        case tok_identifier:
            return ParseIdentifierExpr();

        case tok_number:
            return ParseNumberExpr();

        case '(':
            return ParseParenExpr();
    }
}


// binoprhs
//      ::= ('+' primary)*
// NOTE(srp): ExprPrec is the minimum precedence to 'split' the expression.
// That way, in the expression a+b*c, b*c won't be split under +
internal std::unique_ptr<ExprAST>
ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
{
    for (;;)
    {
        int TokPrec = GetTokPrecedence();

        // NOTE(srp): if binoprhs is empty, it returns the expression passed into it
        if (TokPrec < ExprPrec)
        {
            return LHS;
        }

        int BinOp = CurTok;
        getNextToken(); // eat binop

        auto RHS = ParsePrimary();

        if (!RHS)
        {
            return nullptr;
        }

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec)
        {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS)
            {
                return nullptr;
            }
        }

        LHS  = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

// expression
//      ::= primary binoprhs
internal std::unique_ptr<ExprAST>
ParseExpression()
{
    auto LHS = ParsePrimary();
    if (!LHS)
    {
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(LHS));
}

// prototype
//      ::= id '(' id* ')'
internal std::unique_ptr<PrototypeAST>
ParsePrototype()
{
    if (CurTok != tok_identifier)
    {
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
    {
        return LogErrorP("Expected '(' in prototype");
    }

    std::vector<std::string> ArgNames;

    // NOTE(srp): Modified this while loop because the guide forgot to implement
    // multiple arguments in a function definition
    getNextToken();
    while (CurTok == tok_identifier || CurTok == ',')
    {
        if (CurTok == tok_identifier)
        {
            ArgNames.push_back(IdentifierStr);
        }
        getNextToken();
    }

    if (CurTok != ')')
    {
        return LogErrorP("Expected ')' in prototype");
    }

    getNextToken(); // eat ')'

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// definition 
//      ::= 'def' prototype expression
internal std::unique_ptr<FunctionAST>
ParseDefinition()
{
    getNextToken(); // eat 'def'

    auto Proto = ParsePrototype();
    if (!Proto)
    {
        return nullptr;
    }

    if (auto E = ParseExpression())
    {
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }

    return nullptr;
}

// external
//      ::= 'extern' prototype
internal std::unique_ptr<PrototypeAST> 
ParseExtern()
{
    getNextToken(); // eat extern
    return ParsePrototype();
}

internal std::unique_ptr<FunctionAST> 
ParseTopLevelExpr()
{
    if (auto E = ParseExpression())
    {
        auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }

    return nullptr;
}

// ####################------------------------------------------------####################
//                            T O P  L V L  H A N D L I N G
// ####################------------------------------------------------####################

internal void
HandleDefinition()
{
    if (ParseDefinition())
    {
        fprintf(stderr, "Parsed a function definition.\n");
    }
    else
    {
        // Skip token for error recovery
        getNextToken();
    }
}

internal void
HandleExtern()
{
    if (ParseExtern())
    {
        fprintf(stderr, "Parsed an extern.\n");
    }
    else
    {
        // Skip token for error recovery
        getNextToken();
    }
}

internal void
HandleTopLevelExpression()
{
    if (ParseTopLevelExpr())
    {
        fprintf(stderr, "Parsed a top-level expr.\n");
    }
    else
    {
        // Skip token for error recovery
        getNextToken();
    }
}

internal void
DriveTopLevelInterpreter()
{
    fprintf(stderr, "ready> ");
    getNextToken();

    for (;;)
    {
        fprintf(stderr, "ready> ");
        switch (CurTok)
        {
            case tok_eof:
                return;

            case ';':
                getNextToken();
                break;

            case tok_def:
                HandleDefinition();
                break;

            case tok_extern:
                HandleExtern();
                break;

            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

// ####################------------------------------------------------####################
//                                 B I N O P S  P R E C .
// ####################------------------------------------------------####################

inline void
InstallStandardBinaryOperators()
{
    // 1 is lowest precedence
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40; // highest
}


// ####################------------------------------------------------####################
//                                       M A I N  
// ####################------------------------------------------------####################

int
main()
{
    InstallStandardBinaryOperators();

    DriveTopLevelInterpreter();

    return 0;
}



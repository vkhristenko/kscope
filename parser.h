#ifndef parser_h
#define parser_h

#include <iostream>
#include <vector>
#include <map>

#include "llvm/IR/Verifier.h"

#include "ast.h"
#include "lexer.h"

// function declarations
static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST> ParseIfExpr();
static std::unique_ptr<ExprAST> ParseForExpr();

//
// provide a simple token buffer
// CurTok is the current token the parser is looking at. getNexToken reads
// another token from the lexer and updates CurTok with its results
//
static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}

// helper funcs
std::unique_ptr<ExprAST> LogError(char const* Str) {
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(char const* Str) {
    LogError(Str);
    return nullptr;
}

// numberepxr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // consume the number
    return Result;
}

// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenAST() {
    getNextToken(); // eat (
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat )
    return V;
}

// identifierexpr
//   ::= identifier
//   ::= identifier '(' expression ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier
    if (CurTok != '(')
        return std::make_unique<VariableExprAST>(IdName);

    // Call
    getNextToken(); // eat (
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (1) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else 
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    getNextToken();
    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// primary
//   ::= identifierexpr
//   ::= numberexpr
//   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
    default:
        return LogError("unknown token when expecting an expression");
    case tok_identifier:
        return ParseIdentifierExpr();
    case tok_number:
        return ParseNumberExpr();
    case '(':
        return ParseParenAST();
    case tok_if:
        return ParseIfExpr();
    case tok_for:
        return ParseForExpr();
    }
}

// BinopPrecedence - this holds the precedence for each binary operator that is defined
static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
    if (!isascii(CurTok))
        return -1;

    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

// binoprhs
//   ::= ("+" primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
    // if this is a binop, find its precedence
    while(1) {
        int TokPrec = GetTokPrecedence();

        // if this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done
        if (TokPrec < ExprPrec)
            return LHS;

        // okey, we know this is a binop
        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        // if BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS  as its LHS.
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        // merge LHS/RHS
        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                                              std::move(RHS));

    }
}

// expression
//   ::= primary binoprhs
static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

// function prototype
// prototype
//   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier) 
        return LogErrorP("Expected function anme in prototype");

    std::string FnName = IdentifierStr;
    getNextToken();

    std::cout << FnName.c_str() << std::endl;
    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while(getNextToken() == tok_identifier)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    // success
    getNextToken(); // eat ')'.

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern
    return ParsePrototype();
}


// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        // make an anonymous proto
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", 
                                                    std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }

    return nullptr;
}

// ifexpr ::= 'if' expression 'then' expression 'else' expression
static std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken();

    // condition
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != tok_then)
        return LogError("expected then");
    getNextToken();

    auto Then = ParseExpression();
    if (!Then)
        return nullptr;

    if (CurTok != tok_else)
        return LogError("expected else");

    getNextToken();

    auto Else = ParseExpression();
    if (!Else)
        return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();

    if (CurTok != tok_identifier)
        return LogError("expected identifier after for");

    std::string IdName = IdentifierStr;
    getNextToken();

    if (CurTok != '=')
        return LogError("expected '=' after for");
    getNextToken();

    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ',')
        return LogError("expected ',' after for start value");
    getNextToken();
    
    auto End = ParseExpression();
    if (!End)
        return nullptr;

    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }

    if (CurTok != tok_in)
        return LogError("expected 'in' after for");
    getNextToken();

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    return std::make_unique<ForExprAST>(IdName, std::move(Start),
                                        std::move(End), std::move(Step),
                                        std::move(Body));
}

#endif // parser_h

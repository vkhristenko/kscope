#ifndef ast_h
#define ast_h

#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"

#include <string>
#include <vector>

class ExprAST {
public:
    virtual ~ExprAST() {}
    virtual llvm::Value *codegen() = 0;
};

// expression class for numeric literals like 1.0
class NumberExprAST : public ExprAST {
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}
    virtual llvm::Value *codegen();
};

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;

public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
              std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else))
    {}

    virtual llvm::Value *codegen() override;
};

class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
    ForExprAST(std::string const& VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body))
    {}

    llvm::Value *codegen() override;
};

class UnaryExprAST : public ExprAST {
    char Opcode;
    std::unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
        : Opcode(Opcode), Operand(std::move(Operand)) 
    {}

    virtual llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(std::string const& Name) : Name(Name) {}
    virtual llvm::Value *codegen();
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
        : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

    virtual llvm::Value *codegen();
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(std::string const& Callee,
                std::vector<std::unique_ptr<ExprAST>> Args) 
        : Callee(Callee), Args(std::move(Args)) 
    {}

    virtual llvm::Value *codegen();
};

// this class represents the prototype for a function
// which captures its name, and its argument anmes (thus implicitly tne number of 
// arguments the function takes)
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
    bool IsOperator;
    unsigned Precedence;

public:
    PrototypeAST(std::string const& name, std::vector<std::string> Args,
                 bool IsOperator=false, unsigned Prec = 0)
        : Name(name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec) {}

    std::string const& getName() const { return Name;}
    llvm::Function *codegen();

    bool isUnaryOp() const { return IsOperator and Args.size() == 1; }
    bool isBinaryOp() const { return IsOperator and Args.size() == 2; }

    char getOperatorName() const {
        assert(isUnaryOp() or isBinaryOp());
        return Name[Name.size() - 1];
    }

    unsigned getBinaryPrecedence() const { return Precedence; }
};

// this class represents a function definition itself
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body) 
        : Proto(std::move(Proto)), Body(std::move(Body)) {}

    llvm::Function *codegen();
};

#endif // ast_h

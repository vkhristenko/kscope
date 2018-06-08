#ifndef ast_h
#define ast_h

#include <string>
#include <vector>

class ExprAST {
public:
    virtual ~ExprAST() {}
};

// expression class for numeric literals like 1.0
class NumberExprAST : public ExprAST {
    double Val;

public:
    NumberExprAST(double Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(std::string const& Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
        : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(std::string const& Callee,
                std::vector<std::unique_ptr<ExprAST>> Args) 
        : Callee(Callee), Args(std::move(Args)) 
    {}
};

// this class represents the prototype for a function
// which captures its name, and its argument anmes (thus implicitly tne number of 
// arguments the function takes)
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    Prototype(std::string const& name, std::vector<std::string> Args)
        : Name(name), Args(std::move(Args)) {}

    std::string const& getName() const { return Name;}
};

// this class represents a function definition itself
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::unique_ptr<ExprAST> Body) 
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
};

#endif // ast_h

#ifndef codegen_h
#define codegen_h

#include "Kaleidoscope/include/Kaleidoscope.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include "parser.h"

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> Builder(TheContext);
static std::unique_ptr<llvm::Module> TheModule;
static std::map<std::string, llvm::Value*> NamedValues;
static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<KaleidoscopeJIT> TheJIT;

llvm::Value *LogErrorV(char const* Str) {
    LogError(Str);
    return nullptr;
}

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
    // look this variable up in the function
    llvm::Value *V = NamedValues[Name];
    if (!V)
        LogErrorV("unknown variable name");
    return V;
}

llvm::Value *BinaryExprAST::codegen() {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();

    if (!L or !R)
        return nullptr;

    switch (Op) {
    case '+':
        return Builder.CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder.CreateFSub(L, R, "subtmp");
    case '*':
        return Builder.CreateFMul(L, R, "multmp");
    case '<':
        L = Builder.CreateFCmpULT(L, R, "cmptmp");
        return Builder.CreateUIToFP(L, llvm::Type::getDoubleTy(TheContext),
                                    "booltmp");
    default:
        return LogErrorV("invalid binary operator");
    }
}

llvm::Value *CallExprAST::codegen() {
    // look up the name in the global module table
    llvm::Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("unknown function referenced");

    // if argument mismatch error
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i=0, e=Args.size(); i!=e; i++) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function *PrototypeAST::codegen() {
    // make the function type: double(double, double) etc
    std::vector<llvm::Type*> Doubles(Args.size(), 
                               llvm::Type::getDoubleTy(TheContext));

    llvm::FunctionType *FT = 
        llvm::FunctionType::get(llvm::Type::getDoubleTy(TheContext), Doubles, false);

    llvm::Function *F = 
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, 
                               TheModule.get());

    // set names for all arguments
    unsigned Idx = 0;
    for (auto &Arg: F->args())
        Arg.setName(Args[Idx++]);

    return F;
}

llvm::Function *FunctionAST::codegen() {
    // first, check for an existing function from a previous 'extern' declaration
    llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = Proto->codegen();

    if (!TheFunction)
        return nullptr;

    if (!TheFunction->empty())
        return (llvm::Function*)LogErrorV("function can not be redefined.");

    // create a new basic block to start insertion into
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    // record the function arguments in the NamedValues map
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
        NamedValues[Arg.getName()] = &Arg;

    if (llvm::Value *RetVal = Body->codegen()) {
        // finish off the function
        Builder.CreateRet(RetVal);

        // validate the generated code ,checking for consistency
        llvm::verifyFunction(*TheFunction);

        // run the optimization passes
        TheFPM->run(*TheFunctions);

        return TheFunction;
    }

    // error reading body remove function
    TheFunction->eraseFromParent();
    return nullptr;
}

llvm::Function *getFunction(std::string Name) {
    if (auto *F = TheModule)
}

//
// optimization passes
//
void InitializeModuleAndPassManager(void) {
    // open a new module
    TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
    TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

    // create a new pass manager attached to it
    TheFPM = std::make_unique<llvm::FunctionPassManager>(TheModule.get());

    // simple "peephole" optimizations and bit-twiddling opts
    TheFPM->add(creatInstructionCombiningPass());

    // reassociate exprs
    TheFPM->add(createReassociatePass());

    // eliminate common sub exprs
    TheFPM->add(createGVNPass());

    // simplify control flow graph (deleting unreachable blocks)
    TheFPM->add(createCFGSimplificationPass());

    TheFPM->doInitialization();
}

#endif // codegen_h

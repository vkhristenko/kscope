#ifndef codegen_h
#define codegen_h

#include "include/KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"


#include "parser.h"

llvm::Function *getFunction(std::string Name);

using namespace llvm;
using namespace llvm::orc;

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> Builder(TheContext);
static std::unique_ptr<llvm::Module> TheModule;
static std::map<std::string, llvm::AllocaInst*> NamedValues;
static std::unique_ptr<llvm::legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

llvm::Value *LogErrorV(char const* Str) {
    LogError(Str);
    return nullptr;
}

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(TheContext, llvm::APFloat(Val));
}

static llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, 
                                                std::string const& VarName) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                           TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), 0, VarName.c_str());
}

llvm::Value *VariableExprAST::codegen() {
    // look this variable up in the function
    llvm::Value *V = NamedValues[Name];
    if (!V)
        LogErrorV("unknown variable name");
    return Builder.CreateLoad(V, Name.c_str());
}

llvm::Value *ForExprAST::codegen() {
    // make the new basic block for the loop header, inserting after current
    // block.
    llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();

    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    
    // emit the start code first, without variable in scope
    llvm::Value *StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    // store the value in the alloca
    Builder.CreateStore(StartVal, Alloca);

    // 
    //llvm::BasicBlock *PreheaderBB = Builder.GetInsertBlock();
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(TheContext, "loop", TheFunction);

    // insert an explicit fall through from the current block to the LoopBB
    Builder.CreateBr(LoopBB);

    // start insertion in LoopBB
    Builder.SetInsertPoint(LoopBB);

    // start the PHI node with an entry for Start
    //auto *Variable = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, VarName.c_str());
    //Variable->addIncoming(StartVal, PreheaderBB);

    // within the loop, the variable is defined equal to the PHI node. If it 
    // shadows an existing variable, we have to restore it, so save it now
    //llvm::Value *OldVal = NamedValues[VarName];
    //NamedValues[VarName] = Variable;

    // within the loop, the variable is idefined equal to the PHI node. If it shadows
    // an existing variable, we have to restore it, so save it now
    AllocaInst *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // emit the body of the loop. This, like any other expr, can change the 
    // current BB. Note that we ignore the value computed by the body, but don't 
    // allow an error
    if (!Body->codegen())
        return nullptr;

    // emit the step value
    llvm::Value *StepVal = nullptr;
    if (Step) {
        StepVal = Step->codegen();
        if (!StepVal)
            return nullptr;
    } else {
        // if not specified, use 1.0
        StepVal = ConstantFP::get(TheContext, APFloat(1.0));
    }

    // compute the end condition
    llvm::Value *EndCond = End->codegen();
    if (!EndCond)
        return nullptr;
    
    // reload, increment, and restore the alloca. This handles the case where
    // the body of the loop mutates the variable
    llvm::Value *CurVar = Builder.CreateLoad(Alloca, VarName.c_str());
    llvm::Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
    Builder.CreateStore(NextVar, Alloca);

    // convert condition to a bool by comparing non-equal to 0.0
    EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)),
        "loopcond");

    // create the after looop block and insert it
    llvm::BasicBlock *AfterBB = 
        llvm::BasicBlock::Create(TheContext, "afterloop", TheFunction);

    // insert the conditional branch into the end of LoopEndBB
    Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

    // any new code will be inserted in AfterBB
    Builder.SetInsertPoint(AfterBB);

    // restore the unshadowed variable
    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);

    // for expr always returns 0.0
    return Constant::getNullValue(Type::getDoubleTy(TheContext));
}

llvm::Value *UnaryExprAST::codegen() {
    llvm::Value *OperandV = Operand->codegen();
    if (!OperandV)
        return nullptr;

    llvm::Function *F = getFunction(std::string("unary") + Opcode);
    if (!F)
        return LogErrorV("unknown unary operator");

    return Builder.CreateCall(F, OperandV, "unop");
}

llvm::Value *VarExprAST::codegen() {
    std::vector<AllocaInst*> OldBindings;

    llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();

    // register all variables and emit their initializer
    for (unsigned i=0, e=VarNames.size(); i!=e; ++i) {
        std::string const& VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        llvm::Value *InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        } else {
            // if not specified, use 0.0
            InitVal = ConstantFP::get(TheContext, APFloat(0.0));
        }

        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder.CreateStore(InitVal, Alloca);

        // remember the old variable binding so that we can restore the binding
        // when we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // remember this binding
        NamedValues[VarName] = Alloca;
    }

    // codegen the body, now that all vars are in scope
    llvm::Value *BodyVal = Body->codegen();
    if (!BodyVal)
        return nullptr;

    // pop all our variables from scope
    for (unsigned i = 0, e = VarNames.size(); i!=e; ++i)
        NamedValues[VarNames[i].first] = OldBindings[i];

    // return the body computation
    return BodyVal;
}

llvm::Value *IfExprAST::codegen() {
    llvm::Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    // convert condition to a bool by comparing non-equal to 0.0
    CondV = Builder.CreateFCmpONE(
        CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

    llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();

    // create blocks for the then and else cases. Insert the 'then' block at the 
    // end of the function
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(TheContext, "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(TheContext, "ifcont");

    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // emit then value
    Builder.SetInsertPoint(ThenBB);

    llvm::Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;

    Builder.CreateBr(MergeBB);
    // codegen of 'Then' can cahnge the current block, update ThenBB for the PHI
    ThenBB = Builder.GetInsertBlock();

    // emit else block
    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);

    llvm::Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;

    Builder.CreateBr(MergeBB);
    // codegen of 'Else' can change the current block, update ElseBB for the PHI
    ElseBB = Builder.GetInsertBlock();

    // emit merge block
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

llvm::Value *BinaryExprAST::codegen() {
    // special case '=' because we don't want to emit the LHS as an expression
    if (Op == '=') {
        VariableExprAST *LHSE = dynamic_cast<VariableExprAST*>(LHS.get());
        if (!LHSE)
            return LogErrorV("destination of '=' must be a variable");

        // codegen the RHS
        llvm::Value *Val = RHS->codegen();
        if (!Val)
            return nullptr;

        // loop up the naem
        auto *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
            return LogErrorV("unknown variable name");

        Builder.CreateStore(Val, Variable);
        return Val;
    }

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
        break;
    }

    // if it was not a builtin binary operator, it must be a user defined one. 
    // emit a call to it.
    llvm::Function *F = getFunction(std::string("binary") + Op);
    assert(F and "binary operator not found");

    llvm::Value *Ops[2] = {L, R};
    return Builder.CreateCall(F, Ops, "binop");
}

llvm::Function *getFunction(std::string Name) {
    // first, see if the function has already been added to the current module
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // if not, check whether we can codegen the decl from some existing proto
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // if no existing proto exists, return null
    return nullptr;
}

llvm::Value *CallExprAST::codegen() {
    // look up the name in the global module table
    llvm::Function *CalleeF = getFunction(Callee);
//    llvm::Function *CalleeF = TheModule->getFunction(Callee);
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
//    llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());

    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    llvm::Function *TheFunction = getFunction(P.getName());

//    if (!TheFunction)
//        TheFunction = Proto->codegen();

    if (!TheFunction)
        return nullptr;

    if (!TheFunction->empty())
        return (llvm::Function*)LogErrorV("function can not be redefined.");

    // if this is an opaertor, insatll it
    if (P.isBinaryOp())
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    // create a new basic block to start insertion into
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    // record the function arguments in the NamedValues map
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        // create an alloca for this variable
        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

        // store the initial value into the alloca
        Builder.CreateStore(&Arg, Alloca);

        // add arguments to variable symbol table
        NamedValues[Arg.getName()] = Alloca;
    }

    if (llvm::Value *RetVal = Body->codegen()) {
        // finish off the function
        Builder.CreateRet(RetVal);

        // validate the generated code ,checking for consistency
        llvm::verifyFunction(*TheFunction);

        // run the optimization passes
        TheFPM->run(*TheFunction);

        return TheFunction;
    }

    // error reading body remove function
    TheFunction->eraseFromParent();
    return nullptr;
}

//
// optimization passes
//
void InitializeModuleAndPassManager(void) {
    // open a new module
    TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
    TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

    // create a new pass manager attached to itc
    TheFPM = std::make_unique<llvm::legacy::FunctionPassManager>(TheModule.get());
    
    // promote allocas to registers
    TheFPM->add(createPromoteMemoryToRegisterPass());


    // simple "peephole" optimizations and bit-twiddling opts
    TheFPM->add(createInstructionCombiningPass());

    // reassociate exprs
    TheFPM->add(createReassociatePass());

    // eliminate common sub exprs
    TheFPM->add(createGVNPass());

    // simplify control flow graph (deleting unreachable blocks)
    TheFPM->add(createCFGSimplificationPass());

    TheFPM->doInitialization();
}

static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");

            TheJIT->addModule(std::move(TheModule));
            InitializeModuleAndPassManager();
        }
    } else {
        // skip token for error recovery
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "read extern: ");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");

            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    } else {
        // skip token for error recovery
        getNextToken();
    }
}

static void HandleTopLevelExpression() {
    if (auto FnAST = ParseTopLevelExpr()) {
        if (auto *FnIR = FnAST->codegen()) {
            
            fprintf(stderr, "read top-level expresssion: ");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");

            // jit the module containing the anonymous expr, 
            // keeping a handle to free it later
            auto H = TheJIT->addModule(std::move(TheModule));
            InitializeModuleAndPassManager();

            // search the jit for __anon_expr symbol
            auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
            assert(ExprSymbol && "function not found");

            // get the symbol's address and cast it to the right type
            double (*FP)() = (double (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
            fprintf(stderr, "evaluated to %f\n", FP());

            TheJIT->removeModule(H);
        }
    } else {
        // skip token for error recovery
        getNextToken();
    }
}

// top ::= definition | external | expression | ';'
static  void MainLoop() {
    while (1) {
        fprintf(stderr, "ready> ");
        switch (CurTok) {
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

#endif // codegen_h

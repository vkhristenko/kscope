#include <iostream>

#include "parser.h"
#include "codegen.h"

int main(int argc, char **argv) {
    //
    // native stuff
    //
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // 
    // install binary ops precedence
    //
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();

    // make the module, which holds all the code
    TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
    TheJIT = std::make_unique<KaleidoscopeJIT>();

    // run the main interpreter
    MainLoop();

    // dump the codegen stuff
    TheModule->print(llvm::errs(), nullptr);

    return 0;
}

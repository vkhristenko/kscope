#include <iostream>

#include "parser.h"
#include "codegen.h"

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
    fputc((char)X, stderr);
    return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
    fprintf(stderr, "%f\n", X);
    return 0;
}

int main(int argc, char **argv) {
    //
    // native stuff
    //
#ifdef KINIT_DEBUG
    std::cout << "initialize various native targets" << std::endl;
#endif
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // 
    // install binary ops precedence
    //
    BinopPrecedence['='] = 2;
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

#ifdef KINIT_DEBUG
    std::cout << "setup the term and get the next token" << std::endl;
#endif
    fprintf(stderr, "ready> ");
    getNextToken();

    // make the module, which holds all the code
//    TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
#ifdef KINIT_DEBUG
    std::cout << "initializing the jit" << std::endl;
#endif
    TheJIT = std::make_unique<KaleidoscopeJIT>();

#ifdef KINIT_DEBUG
    std::cout << "initialize module and pass manager" << std::endl;
#endif
    InitializeModuleAndPassManager(); 

    // run the main interpreter
#ifdef KINIT_DEBUG
    std::cout << "start main loop" << std::endl;
#endif
    MainLoop();

    // dump the codegen stuff
 //   TheModule->print(llvm::errs(), nullptr);

    return 0;
}

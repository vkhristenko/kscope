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
//    TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
    TheJIT = std::make_unique<KaleidoscopeJIT>();

    // run the main interpreter
    MainLoop();

    // dump the codegen stuff
 //   TheModule->print(llvm::errs(), nullptr);

    return 0;
}

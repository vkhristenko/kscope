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

#ifdef KINIT_DEBUG
    std::cout << "initialize module and pass manager" << std::endl;
#endif
    InitializeModuleAndPassManager(); 
    
    // run the main interpreter
#ifdef KINIT_DEBUG
    std::cout << "start main loop" << std::endl;
#endif
    MainLoop();

    //
    // native stuff
    //
#ifdef KINIT_DEBUG
    std::cout << "initialize various native targets" << std::endl;
#endif
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    // print an error and exit if we could not find the requested target
    // this generally occurs if we've forgotten to initialize the TargetRegistry 
    // or we have a bogus target triple
    if (!Target) {
        errs() << Error;
        return 1;
    }

    auto CPU = "generic";
    auto Features = "";

    // 
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<Reloc::Model>();
    auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt,
                                                        RM);
    TheModule->setDataLayout(TheTargetMachine->createDataLayout());

    auto Filename = "output.o";
    std::error_code EC;
    raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

    if (EC) {
        errs() << "could not open file: " << EC.message();
        return 1;
    }

    //
    llvm::legacy::PassManager pass;
    auto FileType = TargetMachine::CGFT_ObjectFile;
    if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
        errs() << "TheTargetMachine can not emit a file of this type";
        return 1;
    }

    //
    pass.run(*TheModule);
    dest.flush();

    outs() << "wrote " << Filename << "\n";

    return 0;
}

#include <iostream>

#include "parser.h"
#include "codegen.h"

int main(int argc, char **argv) {
    // 
    // install binary ops precedence
    //
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 30;
    BinopPrecedence['*'] = 40;

    fprintf(stderr, "ready> ");
    getNextToken();
    MainLoop();

    return 0;
}

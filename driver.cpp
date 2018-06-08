#include <iostream>

#include "lexer.h"

int main(int argc, char **argv) {
    int tok;
    while ((tok = gettok()) != tok_eof) {
        std::cout << "tok = " << tok << std::endl;
    }

    return 0;
}

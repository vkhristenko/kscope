#ifndef lexer_h
#define lexer_h

#include <string>

//
// lexer defs/decls
//
enum Token {
    tok_eof = -1,
    tok_def = -2,
    tok_extern = -3,
    tok_identifier = -4,
    tok_number = -5,
};

static std::string IdentifierStr;
static double NumVal;

// gettok - returns the next token from standard input
static int gettok() {
    static int LastChar = ' ';

    // skip any whitespace
    while(isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) {
        // identifier [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while(isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def") 
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        return tok_identifier;
    }

    if (isdigit(LastChar) || LastChar == '.') {
        // number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while(isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    if (LastChar == '#') {
        do 
            LastChar = getchar();
        while(LastChar != EOF and LastChar != '\n' and LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return LastChar;
}

#endif // lexer_h

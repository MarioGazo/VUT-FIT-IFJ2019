// lexical analysis implementation
// Created by Mário Gažo on 2019-10-03.
//

#include "scanner.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

token_t getToken(FILE* in) {

    // initialization of actual token
    token_t actualToken;
    actualToken.tokenType = Start;
    actualToken.tokenAttribute.intValue = 0;

    // state of parser
    parserState_t state = Start;

    // indentation stack
    dynamic_stack_t indetationStack;
    stackInit(&indetationStack);
    int spaceCount = 0;

    // keyword type in case of keyword
    keywords_t keyword = -1;

    // two digit hexadecimal number in case of \xFF notation
    char hexaNum[2];

    // actual character
    int c;
    while (1) {
        c = getc(in);
        switch (state) {
            case Start: // done
                state = parserStart(in,c,&actualToken);
                continue;

            case Plus: // done
                ungetc(c,in);
                actualToken.tokenType = Plus;          return actualToken; // +

            case Minus: // done
                ungetc(c,in);
                actualToken.tokenType = Minus;         return actualToken; // -

            case Multiply: // done
                ungetc(c,in);
                actualToken.tokenType = Multiply;      return actualToken; // *

            case Colon: // done
                ungetc(c,in);
                actualToken.tokenType = Colon;         return actualToken; // :

            case LeftBracket: // done
                ungetc(c,in);
                actualToken.tokenType = LeftBracket;   return actualToken; // (

            case RightBracket: // done
                ungetc(c,in);
                actualToken.tokenType = RightBracket;  return actualToken; // )

            case Comma: // done
                ungetc(c,in);
                actualToken.tokenType = Comma;         return actualToken; // ,

            case DivideWRest: // done
                if (c == '/') {
                    state = DivideWORest;           continue;
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = DivideWRest;        return actualToken; // /
                }

            case DivideWORest: // done
                ungetc(c,in);
                actualToken.tokenType = DivideWORest;           return actualToken; // //

            case Exclamation: // done
                if (c == '=') {
                    state = NotEqual;               continue;
                } else {
                    ungetc(c,in);
                    state = Error;                  continue;
                }

            case NotEqual: // done
                ungetc(c,in);
                actualToken.tokenType = NotEqual;               return actualToken; // !=

            case Smaller: // done
                if (c == '=') {
                    state = SmallerOrEqual;         continue;
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Smaller;            return actualToken; // <
                }

            case SmallerOrEqual: // done
                ungetc(c,in);
                actualToken.tokenType = SmallerOrEqual;         return actualToken; // <=

            case Bigger: // done
                if (c == '=') {
                    state = BiggerOrEqual;          continue;
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Bigger;             return actualToken; // +
                }

            case BiggerOrEqual: // done
                ungetc(c,in);
                actualToken.tokenType = BiggerOrEqual;          return actualToken; // >=

            case Assign: // done
                if (c == '=') {
                    state = Equals;                 continue;
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Assign;             return actualToken; // =
                }

            case Equals: // done
                ungetc(c,in);
                actualToken.tokenType = Equals;                 return actualToken; // ==

            case CommentStart: // done
                if (c == '\n') {
                    state = CommentEnd;             continue;  // #abcdef\n
                } else {
                    continue; // #abcdef
                }

            case CommentEnd: // done
                state = Start;
                continue;

            case EOL: // done
                if (c != ' ') {
                    ungetc(c,in);
                    state = Start;                 continue;
                } else {
                    spaceCount++;
                    while ((c = getc(in)) == ' ') {
                        spaceCount++;
                    }

                    ungetc(c,in);

                    if (c == '\n') {
                        spaceCount = 0;
                        state = Start;             continue;
                    }

                    if (spaceCount == stackTop(indetationStack)) {
                        state = Start;             continue;
                    } else if (spaceCount > stackTop(indetationStack)) {
                        stackPush(&indetationStack, spaceCount);
                        state = Indent;            continue;
                    } else {
                        while (!stackEmpty(&indetationStack)) {
                            actualToken.tokenAttribute.intValue++;

                            if (spaceCount == stackPop(&indetationStack)) {
                                state = Dedent;    break;
                            }
                        }
                        if (stackEmpty(&indetationStack)) {
                            state = Error;         continue;
                        } else {
                            continue;
                        }
                    }
                }

            case Indent: // done                                                    stack:
                ungetc(c,in);                           // def bla():               0
                actualToken.tokenType = Indent;         //     indented code        0 4
                return actualToken;                     //         indented code    0 4 8

            case Dedent: // done
                ungetc(c,in);                           // while true:              0
                actualToken.tokenType = Dedent;         //     code inside loop     0 4
                return actualToken;                     // code outside loop        0

            case OneQuoteStart: // done
                if (c == '\"') {
                    state = TwoQuoteStart;         continue; // ""
                } else {
                    state = Error;                 continue;
                }

            case TwoQuoteStart: // done
                if (c == '\"') {
                    state = DocumentString;        continue; // """
                } else {
                    state = Error;                 continue;
                }

            case DocumentString: // done
                if (c == '\"') {
                    state = OneQuoteEnd;           continue; // """ abcdefghijklm
                } else {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    } else {
                        continue;
                    }
                }
            case OneQuoteEnd: // done
                if (c == '\"') {
                    state = TwoQuoteEnd;           continue;  // """ abcdefghijklm "
                } else {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,34) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    } else {
                        state = DocumentString;    continue;  // """ abcdefghijklm " abc
                    }
                }

            case TwoQuoteEnd: // done
                if (c == '\"') {
                    state = DocumentStringEnd;     continue;  // """ abcdefghijklm """
                } else {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    } else {
                        state = DocumentString;    continue;  // """ abcdefghijklm "" abc
                    }
                }

            case DocumentStringEnd: // done
                ungetc(c,in);
                actualToken.tokenType = DocumentStringEnd; return actualToken;

            case StringStart: // done
                if (c == '\'') {    // 'abc'
                    state = StringEnd;             continue;
                } else if (c < 32) {
                    state = Error;                 continue;
                } else if (c == '\\') {
                    c = getc(in);
                    switch (c) {
                        case '\"':
                            c = '\"';
                            break;
                        case '\'':
                            c = '\'';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case '\\':
                            c = '\\';
                            break;
                        case 'x':
                            hexaNum[0] = getc(in);
                            if (!isxdigit(hexaNum[0])) {
                                state = Error;      continue;
                            }

                            hexaNum[1] = getc(in);
                            if (!isxdigit(hexaNum[1])) {
                                state = Error;      continue;
                            }

                            c = hexToDecimal(hexaNum);
                        default:
                            ungetc(c,in);
                            c = '\\';
                    }
                }

                if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                    state = ErrorMalloc;
                    continue;
                }
                continue;

            case StringEnd: // done
                ungetc(c,in);
                actualToken.tokenType = StringEnd;             return actualToken;

            case IdOrKw: // done
                if (isalpha(c) || isdigit(c) || c == '_') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    }
                } else {
                    ungetc(c,in);

                    if ((keyword = isKeyword(actualToken.tokenAttribute.word->text)) != -1) {
                        state = Keyword;           continue; // if, else, while
                    }
                    state = Identifier;            continue;
                }

                if (isdigit(c) || c == '_') {
                    state = Identifier;            continue;
                }

                if ((keyword = isKeyword(actualToken.tokenAttribute.word->text)) != -1) {
                    state = Keyword;               continue; // if, else, while
                }

                continue;

            case Identifier: // done
                if (isalpha(c) || isdigit(c) || c == '_') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    }
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Identifier;        return  actualToken;
                }

            case Keyword: // done
                ungetc(c,in);
                // TODO not sure whether save keyword as enum number, or string
                dynamicStringFree(actualToken.tokenAttribute.word);
                actualToken.tokenAttribute.intValue = keyword;
                actualToken.tokenType = Keyword;               return actualToken;

            case Integer: // done
                if (isdigit(c)) {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;
                        continue;
                    } else {
                        continue;
                    }
                } else if (c == '.') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;
                        continue;
                    }
                    if (isdigit(c = getc(in))) {
                        ungetc(c,in);
                        state = Double;             continue;
                    } else {
                        ungetc(c,in);
                        dynamicStringFree(actualToken.tokenAttribute.word);
                        state = Error;              continue;
                    }
                } else if (c == 'e' || c == 'E') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;        continue;
                    } else {
                        state = AlmostExponential;  continue;
                    }
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Integer;

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.intValue = strToInt(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);
                    return actualToken;
                }

            case Double: // done
                if (isdigit(c)) {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;        continue;
                    } else {
                        continue;
                    }
                } else if (c == 'e' || c == 'E') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;        continue;
                    } else {
                        state = AlmostExponential;  continue;
                    }
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Double;

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.doubleValue = strToDouble(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);
                    return actualToken;
                }

            case AlmostExponential: // done
                if (isdigit(c) || c == '+' || c == '-') {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;        continue;
                    } else {
                        state = Exponential;        continue;
                    }
                } else {
                    ungetc(c,in);
                    dynamicStringFree(actualToken.tokenAttribute.word);
                    actualToken.tokenType = Error;
                    continue;
                }

            case Exponential: // done
                if (isdigit(c)) {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;        continue;
                    } else {
                        continue;
                    }
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Double; // every exponential num is converted to double

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.doubleValue = strToDouble(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);
                    return actualToken;
                }

            case BinOctHex: // done
                if (c == 'b' || c == 'B') {
                    state = BinaryNum;             continue; // 0b 0B
                } else if (c == 'o' || c == 'O') {
                    state = OctalNum;              continue; // 0o 0O
                } else if (c == 'x' || c == 'X') {
                    state = HexadecimalNum;        continue; // 0x 0X
                } else if (isdigit(c)) {
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue; // malloc error
                    } else {
                        state = Integer;           continue; // 0123
                    }
                } else {
                    ungetc(c,in);
                    dynamicStringFree(actualToken.tokenAttribute.word);
                    state = Error;                 continue;
                }

            case BinaryNum: // done
                if (c == '0' || c == '1') { // 0b111
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue;
                    } else {
                        continue;
                    }
                }

                if (c == '_') continue;  // 0b1111_0000

                if (isdigit(c)) {
                    dynamicStringFree(actualToken.tokenAttribute.word);
                    state = Error;                 continue;
                } else {
                    ungetc(c, in);
                    actualToken.tokenType = Integer;

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.intValue = binToDecimal(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);
                    return actualToken;
                }

            case OctalNum: // done
                if (c == '0' || c == '1' || c == '2' || c == '3' ||
                    c == '4' || c =='5' || c == '6' || c == '7') { // 0o123
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue;
                    } else {
                        continue;
                    }
                }

                if (c == '_') continue; // 0o123_456

                if (isdigit(c)) {
                    dynamicStringFree(actualToken.tokenAttribute.word);
                    state = Error;                 continue;
                } else {
                    ungetc(c, in);
                    actualToken.tokenType = Integer;

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.intValue = octToDecimal(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);
                    return actualToken;
                }

            case HexadecimalNum: // done
                if (isxdigit(c)) { // 0xFFFF
                    if (dynamicStringAddChar(actualToken.tokenAttribute.word,c) == false) {
                        state = ErrorMalloc;       continue;
                    } else {
                        continue;
                    }
                } else if (c == '_') { // 0xFFFF_FFFF
                    continue;
                } else {
                    ungetc(c,in);
                    actualToken.tokenType = Integer;

                    dynamicString_t* StringNumPtr = actualToken.tokenAttribute.word;
                    actualToken.tokenAttribute.intValue = hexToDecimal(actualToken.tokenAttribute.word->text);
                    dynamicStringFree(StringNumPtr);    return actualToken;
                }

            case Error: // done
                ungetc(c,in);
                actualToken.tokenType = Error;          return actualToken; // lexical error

            case ErrorMalloc: // done
                ungetc(c,in);
                actualToken.tokenType = ErrorMalloc;    return actualToken; // memory allocation error

            case EndOfFile: // done
                ungetc(c,in);
                actualToken.tokenType = EndOfFile;      return actualToken; // EndOfFile

            default: // done
                ungetc(c,in);
                state = Start;                      continue;
        }
    }

}

parserState_t parserStart(FILE* in, int c, token_t* actualToken) {
    parserState_t state = Start;

    if (c == '+') {
        state = Plus;
    } else if (c == '-') {
        state = Minus;
    } else if (c == '*') {
        state = Multiply;
    } else if (c == ':') {
        state = Colon;
    } else if (c == '(') {
        state = LeftBracket;
    } else if (c == ')') {
        state = RightBracket;
    } else if (c == ',') {
        state = Comma;
    } else if (c == EOF) {
        state = EndOfFile;
    } else if (c == '/') {
        state = DivideWRest;
    } else if (c == '!') {
        state = Exclamation;
    } else if (c == '#') {
        state = CommentStart;
    } else if (c == '\n') {
        state = EOL;
    } else if (c == '<') {
        state = Smaller;
    } else if (c == '>') {
        state = Bigger;
    } else if (c == '=') {
        state = Assign;
    } else if (c == ' ') {
        state = Start;
    } else if (c == '\"') {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        } else {
            state = OneQuoteStart;
        }
    }  else if (c == '\'') {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        } else {
            state = StringStart;
        }
    } else if (isdigit(c) && c != '0') {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        }

        if (dynamicStringAddChar(actualToken->tokenAttribute.word,c) == false) {
            state = ErrorMalloc;
        } else {
            state = Integer;
        }
    } else if (c == '0') {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        } else {
            state = BinOctHex;
        }
    } else if (isalpha(c)) {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        }

        if (dynamicStringAddChar(actualToken->tokenAttribute.word,c) == false) {
            state = ErrorMalloc;
        } else {
            state = IdOrKw;
        }
    } else if (c == '_') {
        if (dynamicStringInit(actualToken->tokenAttribute.word) == false) {
            state = ErrorMalloc;
        }

        if (dynamicStringAddChar(actualToken->tokenAttribute.word,c) == false) {
            state = ErrorMalloc;
        } else {
            state = Identifier;
        }
    }  else if (c == ' ') {
            state = Start;
    }  else {
        ungetc(c,in);
        state = Error;
    }

    return state;
}

keywords_t isKeyword(const char* string) {
    keywords_t keyword;

    if (strcmp(string,"def") == 0) {
        keyword = keywordDef;
    } else if (strcmp(string,"if") == 0) {
        keyword = keywordIf;
    } else if (strcmp(string,"else") == 0) {
        keyword = keywordElse;
    } else if (strcmp(string,"None") == 0) {
        keyword = keywordNone;
    } else if (strcmp(string,"pass") == 0) {
        keyword = keywordPass;
    } else if (strcmp(string,"return") == 0) {
        keyword = keywordReturn;
    } else if (strcmp(string,"while") == 0) {
        keyword = keywordWhile;
    } else if (strcmp(string,"inputs") == 0) {
        keyword = keywordInputs;
    } else if (strcmp(string,"inputi") == 0) {
        keyword = keywordInputi;
    } else if (strcmp(string,"inputf") == 0) {
        keyword = keywordInputf;
    } else if (strcmp(string,"print") == 0) {
        keyword = keywordPrint;
    } else if (strcmp(string,"len") == 0) {
        keyword = keywordLen;
    } else if (strcmp(string,"substr") == 0) {
        keyword = keywordSubstr;
    } else if (strcmp(string,"ord") == 0) {
        keyword = keywordOrd;
    } else if (strcmp(string,"char") == 0) {
        keyword = keywordChar;
    } else {
        keyword = notKeyword;
    }

    return keyword;
}

double strToDouble(const char* string) {
    return strtod(string, NULL);
}

int strToInt(const char* string) {
    return strtol(string,NULL,10);
}

int binToDecimal(const char* string) {
    return strtol(string,NULL,2);
}

int octToDecimal(const char* string) {
    return strtol(string, NULL, 8);
}

int hexToDecimal(const char* string) {
    return strtol(string, NULL, 16);
}

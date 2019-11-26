/**
 * Implementation of imperative language IFJ2019 compiler
 * @file parser.c
 * @author Mario Gazo (xgazom00)
 * @brief Parser implementation
 */
#include "parser.h"
#include "scanner.h"
#include "code-gen.h"
#include "dynamic-stack.h"
#include "symtable.h"
#include "error.h"
#include "expression.h"

#define DEBUG 1 // TODO pred odovzdanim nastavit na 0

#define TRUE 1
#define FALSE 0

// Makro kontolujúce, či je token valid
#define GET_TOKEN \
    actualToken = getToken(in,&indentationStack);  \
    if (DEBUG)     \
        printToken(&indentationStack, actualToken); \
    if (actualToken.tokenType == Error) return LEX_ERR;  \
    if (actualToken.tokenType == ErrorMalloc) return INTERNAL_ERR;  \
    if (actualToken.tokenType == ErrorIndent) return SYNTAX_ERR

// Makro kontolujúce, či je typ tokenu požadovaným typom
#define GET_AND_CHECK_TOKEN(type) \
    GET_TOKEN;  \
    if (actualToken.tokenType != type)  \
        return SYNTAX_ERR

#define PRINT_DEBUG(text) \
    if (DEBUG) \
        printf(text)

// Globálne premenné
token_t actualToken;
FILE* in;
dynamic_stack_t indentationStack;
unsigned long uni_a = 0;
unsigned long uni_b = 42;
// Kód chybového hlásenia prekladača
int errorCode;
// Tabuľka symbolov
hashTable *GlobalTable, *LocalTable;

// Príznaky značia kde sa nachádza čítanie programu
bool inFunc = false;    // sme vo funkcii
bool expr = false;      // bol spracovany vyraz
bool inBody = false;    // prebieha citanie instrukcii v tele hlavneho programu

// Výsledný kód
dynamicString_t code;
FILE * output;

// Hlavný program
int analyse(FILE* file) {
    PRINT_DEBUG("Analysis start\n");

    // Inicializácia globálnych premenných
    stackInit(&indentationStack);
    // Rozmery tabuľky prevzaté z internetu
    GlobalTable = TInit(236897);
    LocalTable = TInit(1741);
    // Inicializácia dynamic stringu pre výsledný kód
    dynamicStringInit(&code);

    // Vstupné dáta zo STDIN
    in = file;

    // Nastavime viditelnost vysledneho kodu suboru code-gen.c
    set_code_output(&code);
    // TODO gen main scope start
    cg_main_scope_start();
    // TODO gen built in functions
    cg_define_b_i_functions();
    // Spustenie analýzi
    errorCode = program();
    // TODO gen main scope end
    cg_main_scope_end();

    // zistujeme ci boli vsetky volane funkcie definovane
    for (int i = 0; i < 236897; i++) {
        if (GlobalTable->variables[i] != NULL) {
            if (GlobalTable->variables[i]->type == TypeFunction &&
                GlobalTable->variables[i]->defined != TRUE) {
                return SEMPROG_ERR;
            }
        }
    }

    // Uvoľnenie pamäti
    TFree(GlobalTable);
    TFree(LocalTable);
    stackFree(&indentationStack);

    // Ak je program korektný, je výsledný kód vypísaný do súboru
    if (errorCode == PROG_OK) {
        output = fopen("prg.out","w+");

        // Zlyhanie pri tvorbe súboru na výpis
        if (output == NULL) {
            return INTERNAL_ERR;
        }

        fprintf(output, "%s", code.text);
        dynamicStringFree(&code);
        fclose(output);

        return PROG_OK;
    } else {
        dynamicStringFree(&code);
        return errorCode;
    }
}

// Hlavný mechanizmus spracovávania programu
// Na základe tokenov spracováva telo programu, používateľom definované funkcie
// alebo vstavané funkcie a programové konštrukcie
// Funkcia je opakovane volaná, pokiaľ nie je program spracovaný alebo nie je nájdená chyba
int program() {
    PRINT_DEBUG("Program body\n");

    // Zo scanneru získame ďalší token
    GET_TOKEN;

    // Na základe tokenu sa zvolí vetva pre spracovanie
    // Koniec spracovávaného programu
    if (actualToken.tokenType == EndOfFile) {
        PRINT_DEBUG("End of program\n");

        GET_AND_CHECK_TOKEN(EOL);
        return PROG_OK;
    // Používateľom definovaná funkcia
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordDef) { // func def
        PRINT_DEBUG("Function definition\n");

        inFunc = true;
        if ((errorCode = defFunction()) != PROG_OK) return errorCode;

        return program();
    // DocumentString - je preskočený
    } else if (actualToken.tokenType == DocumentString) {
        PRINT_DEBUG("DocumentString\n");

        GET_AND_CHECK_TOKEN(EOL);
        return program();
    // Programová konštrukcia alebo vstavaná funkcia
    } else {
        inBody = true;
        if ((errorCode = commandList()) != PROG_OK) return errorCode;
        return program();
    }
}

// 1. Vetva - Spracovávanie používateľom definovanej funkcie
int defFunction() {
    // Záznamy funkcie

    hTabItem_t funcRecord; // zaznam funkcie
    hTabItem_t* controlRecord;

    // Prechádzame konštukciu funkcie a kontolujeme syntaktickú správnosť zápisu
    // Vzor: def id ( zoznam_parametrov ) : INDENT
    //          sekvencia príkazov <- EOL po kazdom prikaze
    // DEDENT
    GET_AND_CHECK_TOKEN(Identifier);

    // TODO gen function start
    cg_fun_start(dynamicStringGetText(actualToken.tokenAttribute.word));
    // TODO gen function retval
    cg_fun_retval();
    // TODO gen function frame
    cg_fun_before_params();
    funcRecord.key = actualToken.tokenAttribute.word;
    funcRecord.type = TypeFunction;
    funcRecord.defined = TRUE;

    funcRecord.next = NULL;


    GET_AND_CHECK_TOKEN(LeftBracket);

    // Počet parametrov
    funcRecord.value.intValue = 0;
    if ((errorCode = param(&funcRecord)) != PROG_OK) return errorCode;

    // Ak uz bola funkcia pouzita, je v HashTable, skontolujeme len pocet parametrov
    if ((controlRecord = TSearch(GlobalTable, funcRecord.key)) != NULL){
        // Redefinicia funkcie
        if (controlRecord->defined) return SEMPROG_ERR;

        // Funkcia uz bola pouzita, skontrolujeme, ci ma rovnaky pocet parametrov
        if (controlRecord->value.intValue != funcRecord.value.intValue){
            return SEMPARAM_ERR;
        } else {
            inFunc = false;
            return PROG_OK;
        }
    }

    GET_AND_CHECK_TOKEN(Colon);                                         // def foo(...):
    GET_AND_CHECK_TOKEN(Indent);                                        // def foo(...):

    GET_TOKEN;
    if ((errorCode = commandList()) != PROG_OK) return errorCode;       // __command_list

    // Uložíme funkciu do HashTable
    TInsert(GlobalTable, funcRecord);

    // TODO gen function end
    cg_fun_end(funcRecord.key.text);
    inFunc = false;
    return PROG_OK;
}

// 1a. Vetva - Spracovávanie parametra/parametrov funkcie
int param(hTabItem_t* funcRecord) {
    PRINT_DEBUG("Parameters\n");

    GET_TOKEN;

    // Spracovávame parametre funkcie a zároveň si ukladáme ich počet
    if (actualToken.tokenType == RightBracket) {            // def foo()
        return PROG_OK;
    } else if (actualToken.tokenType == Identifier) {
        funcRecord->value.intValue++;

        // TODO gen function param
        ADD_INST("DEFVAR LF@param"); ADD_CODE(funcRecord->value.intValue);
        ADD_CODE("MOVE LF@param"); ADD_CODE(funcRecord->value.intValue); ADD_CODE(" LF@%"); ADD_CODE(funcRecord->value.intValue);
        GET_TOKEN;

        // Syntaktická kontrola jednotlivých parametrov
        if (actualToken.tokenType == RightBracket) {        // def foo(a)
            return PROG_OK;
        } else if (actualToken.tokenType == Comma) {        // def foo(a,
            return param(funcRecord);
        } else {
            return SYNTAX_ERR;
        }
    } else {
        return SYNTAX_ERR;
    }
}

// 2. Vetva - Spracovávanie programových konštrukcií a vstavaných príkazov
int commandList() {
    PRINT_DEBUG("Command\n");

    // WHILE
    // Vzor: while (výraz): INDENT
    //          sekvencia prikazov <- EOL po kazdom prikaze
    //       DEDENT
    if (actualToken.tokenType == Keyword &&
        actualToken.tokenAttribute.intValue == keywordWhile) {  // while
        PRINT_DEBUG("\tWhile\n");

        // TODO while loop head
        // TODO while loop start
        cg_while_start(uni_a, uni_b);

        if ((errorCode = expression(in,&indentationStack, NULL)) != PROG_OK) return errorCode;  // while <expr>

        if (actualToken.tokenType != Colon)                     // while <expr>:
            return SYNTAX_ERR;

        GET_AND_CHECK_TOKEN(Indent);                            // while <expr>:
        // __command_list
        if ((errorCode = commandList()) != PROG_OK) return errorCode;

        // TODO while loop end
        cg_while_end(uni_a, uni_b);
        uni_a += 1;
        uni_b += 1;

        return (errorCode = commandListContOrEnd());
    // IF & ELSE
    // Vzor:  if (výraz): INDENT
    //          sekvencia príkazov <- EOL po kazdom prikaze
    //        DEDENT else: INDENT
    //          sekvencia prikazov <- EOL po kazdom prikaze
    //        DEDENT
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordIf) {
        PRINT_DEBUG("\tIf & Else\n");

        // TODO if & else head
        // TODO if & else start
        cg_if_start(uni_a, uni_b);

        if ((errorCode = expression(in,&indentationStack, NULL)) != PROG_OK) return errorCode;  // if <expr>

        if (actualToken.tokenType != Colon)                     // if <expr>:
            return SYNTAX_ERR;

        GET_AND_CHECK_TOKEN(Indent);                            // if <expr>:
                                                                // __command_list
        if ((errorCode = commandList()) != PROG_OK) return errorCode;


        // TODO else part
        cg_if_else_part(uni_a, uni_b);

        GET_AND_CHECK_TOKEN(Keyword);
        if (actualToken.tokenAttribute.intValue != keywordElse) // if <expr>:
            return SYNTAX_ERR;                                  // __command_list
                                                                // else:
        GET_AND_CHECK_TOKEN(Colon);                             // __command_list
        GET_AND_CHECK_TOKEN(Indent);                            //

        if ((errorCode = commandList()) != PROG_OK) return errorCode;

        // TODO if & else end
        cg_if_end(uni_a, uni_b);
        uni_a += 1;
        uni_b += 1;

        return (errorCode = commandListContOrEnd());
    // PRINT
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordPrint) {
        PRINT_DEBUG("\tPrint\n");

        GET_AND_CHECK_TOKEN(LeftBracket);                       // print(
        PRINT_DEBUG("Terms\n");
        if ((errorCode = term()) != PROG_OK) return errorCode;  // print(...)

        return (errorCode = commandListContOrEnd());
    // RETURN
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordReturn) {
        PRINT_DEBUG("\tReturn\n");

        if (!inFunc) return SYNTAX_ERR;

        GET_TOKEN;

        expr = true;
        if ((errorCode = expression(in,&indentationStack, &actualToken)) != 0) return errorCode;     // value != None

        // TODO RETURN TMP -> tmp vysledok
        cg_fun_return();

        return (errorCode = commandListContOrEnd());
    // PASS
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordPass) {
        PRINT_DEBUG("\tPass\n");
        // A: TODO PASS
        // Q: Čo sa tú ma vypisovať?
        // A: Asi nič.
        // Q: A sme si istý?
        // A: Nie.
        return (errorCode = commandListContOrEnd());
    // ID
    } else if (actualToken.tokenType == Identifier) {   // abc....abc = <value> / abc()
        PRINT_DEBUG("\tID\n");

        hTabItem_t varRecord; // zaznam premennej
        varRecord.key = actualToken.tokenAttribute.word;
        varRecord.defined = TRUE;
        varRecord.next = NULL;

        GET_TOKEN;

        if (actualToken.tokenType == Assign) {

            // TODO defvar var
            if (inFunc) {
                ADD_INST("DEFVAR LF@"); ADD_CODE(varRecord.key);
            } else {
                ADD_INST("DEFVAR GF@"); ADD_CODE(varRecord.key);
            }

            if ((errorCode = assign(&varRecord)) != PROG_OK) return errorCode;
            // Uloženie premennej do HashTable
            if (inFunc) {
                TInsert(LocalTable, varRecord);
            } else {
                TInsert(GlobalTable, varRecord);
            }

            return (errorCode = commandListContOrEnd());
        } else if (actualToken.tokenType == LeftBracket) {

            param(&varRecord);
            // Ide o funkciu, ak je definovaná, presvedčíme sa že sedí počet parametrov, inak ju pridáme do hTab
            hTabItem_t* funcRec = NULL;
            if ((funcRec = (TSearch(GlobalTable,varRecord.key))) != NULL) {
                if (funcRec->value.intValue != varRecord.value.intValue)
                    return SEMPARAM_ERR;
            } else {
                varRecord.type = TypeFunction;
                varRecord.defined = FALSE;
                TInsert(GlobalTable,varRecord);
            }
            cg_fun_call(funcRec->key.text);
            return (errorCode = commandListContOrEnd());
        } else {
            return SYNTAX_ERR;
        }
    // ERROR
    } else {
        return SYNTAX_ERR;
    }
}

int term() {
    GET_TOKEN;

    if (actualToken.tokenType == DocumentString) {
        cg_print(actualToken.tokenAttribute.word.text, TypeString);
    } else if (actualToken.tokenType == Identifier) {
        hTabItem_t *var;
        if ((var = TSearch(GlobalTable,actualToken.tokenAttribute.word)) != NULL) {
            if (cg_frame('G') == false)                  return INTERNAL_ERR;
            if (cg_print(var->key.text,var->type) == false)     return INTERNAL_ERR;
        } else if ((var = TSearch(LocalTable,actualToken.tokenAttribute.word)) != NULL) {
            if (cg_frame('L') == false)                  return INTERNAL_ERR;
            if (cg_print(var->key.text,var->type) == false)     return INTERNAL_ERR;
        } else {
            return SEMPROG_ERR; // nedefinovana premenna
        }
    } else if (actualToken.tokenType == String) {
        if (cg_print(actualToken.tokenAttribute.word.text, TypeString) == false) return INTERNAL_ERR;
    } else if (actualToken.tokenType == Integer) {
        char buffer[100];
        sprintf(buffer,"%d",actualToken.tokenAttribute.intValue);
        if (cg_print(buffer, TypeInteger) == false)             return INTERNAL_ERR;
    } else if (actualToken.tokenType == Double) {
        char buffer[100];
        sprintf(buffer,"%a",actualToken.tokenAttribute.doubleValue);
        if (cg_print(buffer, TypeDouble) == false)              return INTERNAL_ERR;
    } else if (actualToken.tokenType == Keyword && actualToken.tokenAttribute.intValue == keywordNone) {
        if (cg_print("None", TypeNone) == false)                return INTERNAL_ERR;
    } else {
        return SYNTAX_ERR;
    }

    GET_TOKEN;

    if (actualToken.tokenType == RightBracket) {
        if (cg_print("\n", TypeString) == false) return INTERNAL_ERR;
        return PROG_OK;
    } else if (actualToken.tokenType == Comma) {
        if (cg_print(" ", TypeString) == false)  return INTERNAL_ERR;
        return (errorCode = term());
    } else {
        return SYNTAX_ERR;
    }
}

int assign(hTabItem_t* varRecord) {
    PRINT_DEBUG("Assignment\n");

    GET_TOKEN;

    // ID
    if (actualToken.tokenType == Identifier) { //abc = abc...
        PRINT_DEBUG("\tID\n");


        GET_TOKEN;
        //Volanie funkcie
        if (actualToken.tokenType == LeftBracket) { //abc = a(
            hTabItem_t funcRecord;
            hTabItem_t *controlRecord;

            // Musime skontrolovat, ci bola funkcia definovana a ak ano, ci sedi pocet parametrov
            funcRecord.value.intValue = 0;
            funcRecord.key = actualToken.tokenAttribute.word;
            funcRecord.type = TypeFunction;
            funcRecord.defined = FALSE;
            funcRecord.next = NULL;

            if ((errorCode = param(&funcRecord)) != PROG_OK) return errorCode;  // //abc = a(...)

            if ((controlRecord = TSearch(GlobalTable, funcRecord.key)) != NULL) {
                // Funkcia uz bola definovana, skontrolujeme, ci ma rovnaky pocet parametrov
                if (controlRecord->value.intValue != funcRecord.value.intValue)
                    return SEMPARAM_ERR;
            } else {
                // Uložíme funkciu do HashTable a budeme neskor zistovat jej definiciu
                TInsert(GlobalTable, funcRecord);
            }

            // TODO function call
            cg_fun_call(funcRecord.key);
            // TODO assign return value
            if (inFunc) {
                ADD_INST("MOVE LF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
            } else {
                ADD_INST("MOVE GF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
            }
        } else {
            // Volanie precedentnej analyzi
            // Riesi sa vyraz, musime odovzdat dva tokeny
            // controlToken a actualToken
            expr = true;
            if ((errorCode = expression(in,&indentationStack, NULL)) != 0) return  errorCode;

            // TODO MOVE var TMP -> tmp vysledok
            if (inFunc) {
                ADD_INST("MOVE LF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
            } else {
                ADD_INST("MOVE GF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
            }
            return PROG_OK;
        }
        return PROG_OK;
    // EXPRESSION()
    } else if (actualToken.tokenType == Double || actualToken.tokenType == Integer) {
        PRINT_DEBUG("\tExpression\n");

        expr = true;
        if ((errorCode = expression(in,&indentationStack, &actualToken)) != 0) return  errorCode;

        // TODO MOVE var TMP -> tmp vysledok
        if (inFunc) {
            ADD_INST("MOVE LF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
        } else {
            ADD_INST("MOVE GF@"); ADD_CODE(varRecord.key); ADD_CODE(" TF@navratova_hodnota");
        }
        return PROG_OK;
    // INPUTF()
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordInputf) {
        PRINT_DEBUG("\tInputf\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(RightBracket);

        varRecord->type = TypeDouble;
        if (cg_input(*varRecord) == false) return INTERNAL_ERR;
        return PROG_OK;
    // INPUTS()
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordInputs) {
        PRINT_DEBUG("\tInputs\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(RightBracket);

        varRecord->type = TypeString;
        if (cg_input(*varRecord) == false) return INTERNAL_ERR;
        return PROG_OK;
    // INPUTI()
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordInputi) {
        PRINT_DEBUG("\tInputi\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(RightBracket);

        varRecord->type = TypeInteger;
        if (cg_input(*varRecord) == false) return INTERNAL_ERR;
        return PROG_OK;
    // LEN(s)
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordLen) {
        PRINT_DEBUG("\tLen\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(String);
        // TODO MOVE var STRLEN(s)
        ADD_CODE("CREATEFRAME");
        ADD_CODE("DEFVAR TF@%1");
        ADD_CODE("MOVE TF@%1 string@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.word));

        GET_AND_CHECK_TOKEN(RightBracket);

        // TODO function call len
        cg_fun_call("FUNCTION_LEN");

        // TODO function retval assign to var
        if (inFunc) {
            ADD_INST("MOVE LF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        } else {
            ADD_INST("MOVE GF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        }

        varRecord->type = TypeInteger;
        return PROG_OK;
    // SUBSTR(s,i,n)
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordSubstr) {
        PRINT_DEBUG("\tSubstr\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(String);
        // TODO MOVE str String
        ADD_CODE("CREATEFRAME");
        ADD_CODE("DEFVAR TF@%1");
        ADD_CODE("MOVE TF@%1 string@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.word));
        GET_AND_CHECK_TOKEN(Comma);
        GET_AND_CHECK_TOKEN(Integer);
        // TODO MOVE int Int
        ADD_CODE("DEFVAR TF@%2");
        ADD_CODE("MOVE TF@%2 int@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.intValue));
        GET_AND_CHECK_TOKEN(Comma);
        GET_AND_CHECK_TOKEN(Integer);
        // TODO MOVE int Int
        ADD_CODE("DEFVAR TF@%3");
        ADD_CODE("MOVE TF@%3 int@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.intValue));
        GET_AND_CHECK_TOKEN(RightBracket);

        // TODO function call substr
        cg_fun_call("FUNCTION_SUBSTR");
        // TODO function retval assign to var
        if (inFunc) {
            ADD_INST("MOVE LF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        } else {
            ADD_INST("MOVE GF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        }
        varRecord->type = TypeString;
        return PROG_OK;
    // ORD(s,i)
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordOrd) {
        PRINT_DEBUG("\tOrd\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(String);
        // TODO MOVE str String
        ADD_CODE("CREATEFRAME");
        ADD_CODE("DEFVAR TF@%1");
        ADD_CODE("MOVE TF@%1 string@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.word));
        GET_AND_CHECK_TOKEN(Comma);
        GET_AND_CHECK_TOKEN(Integer);
        // TODO MOVE int Int
        ADD_CODE("DEFVAR TF@%2");
        ADD_CODE("MOVE TF@%2 int@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.intValue));
        GET_AND_CHECK_TOKEN(RightBracket);

        // TODO function call ord
        cg_fun_call("FUNCTION_ORD");
        // TODO function retval assign to var
        if (inFunc) {
            ADD_INST("MOVE LF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        } else {
            ADD_INST("MOVE GF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        }
        varRecord->type = TypeInteger;
        return PROG_OK;
    // CHR(i)
    } else if (actualToken.tokenType == Keyword &&
               actualToken.tokenAttribute.intValue == keywordChr) {
        PRINT_DEBUG("\tChr\n");

        GET_AND_CHECK_TOKEN(LeftBracket);
        GET_AND_CHECK_TOKEN(Integer);

        // TODO MOVE str 'Int'
        ADD_CODE("CREATEFRAME");
        ADD_CODE("DEFVAR TF@%1");
        ADD_CODE("MOVE TF@%1 int@"); ADD_CODE(dynamicStringGetText(actualToken.tokenAttribute.intValue));
        GET_AND_CHECK_TOKEN(RightBracket);

        // TODO function call chr
        cg_fun_call("FUNCTION_CHR");
        // TODO function retval assign to var
        if (inFunc) {
            ADD_INST("MOVE LF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        } else {
            ADD_INST("MOVE GF@"); ADD_CODE(varRecord->key); ADD_CODE(" TF@navratova_hodnota");
        }
        varRecord->type = TypeString;
        return PROG_OK;
    // ERROR
    } else {
        return SYNTAX_ERR;
    }
}

int commandListContOrEnd() {
    // Tu sa nič negeneruje
    PRINT_DEBUG("\tAnother command?\n");

    // ak bol priradeny vyraz terminalnym znakom bol bud EOL alebo Dedent
    if (expr) { expr = false; } else { GET_TOKEN; }

    // ak je citany prikaz v tele programu, musi byt zakonceny EOL
    if (inBody && (actualToken.tokenType == EOL || actualToken.tokenType == EndOfFile)) {
        inBody = false;
        return PROG_OK;
    } else if (inBody) {
        return SYNTAX_ERR;
    }

    // ak je citany prikaz v tele cyklu, if-u alebo funkcie, konci pri tokene Dedent a pokracuje pri EOL
    if (actualToken.tokenType == Dedent) {
        return PROG_OK;
    } else if (actualToken.tokenType == EOL) {
        GET_TOKEN;
        return (errorCode = commandList());
    } else {
        return SYNTAX_ERR;
    }
}

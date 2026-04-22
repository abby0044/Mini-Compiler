// #include <stdio.h>
// #include <stdlib.h>
// #include "lexer.h"

// int isKeyword(TokenType type) {
//     return (type == TOKEN_INT ||
//             type == TOKEN_FLOAT ||
//             type == TOKEN_IF ||
//             type == TOKEN_ELSE ||
//             type == TOKEN_WHILE ||
//             type == TOKEN_RETURN ||
//             type == TOKEN_PRINT ||
//             type == TOKEN_MAIN);
// }

// int isOperator(TokenType type) {
//     return (type == TOKEN_PLUS ||
//             type == TOKEN_MINUS ||
//             type == TOKEN_MUL ||
//             type == TOKEN_DIV ||
//             type == TOKEN_ASSIGN ||
//             type == TOKEN_EQ ||
//             type == TOKEN_NEQ ||
//             type == TOKEN_LT ||
//             type == TOKEN_GT ||
//             type == TOKEN_LE ||
//             type == TOKEN_GE);
// }

// int main(int argc, char *argv[]) {

//     if (argc < 2) {
//         printf("Usage: .\\compiler.exe <inputfile>\n");
//         return 1;
//     }

//     FILE *fp = fopen(argv[1], "r");

//     if (!fp) {
//         printf("Error opening file.\n");
//         return 1;
//     }

//     Token token;

//     int totalTokens = 0;
//     int identifierCount = 0;
//     int keywordCount = 0;
//     int operatorCount = 0;
//     int intLiteralCount = 0;
//     int floatLiteralCount = 0;

//     printf("----- TOKEN OUTPUT -----\n\n");

//     do {
//         token = getNextToken(fp);

//         printf("Line %-3d  %-22s  %s\n",
//                token.line,
//                tokenTypeToString(token.type),
//                token.lexeme);

//         if (token.type != TOKEN_EOF)
//             totalTokens++;

//         if (token.type == TOKEN_IDENTIFIER)
//             identifierCount++;

//         if (token.type == TOKEN_INT_LITERAL)
//             intLiteralCount++;

//         if (token.type == TOKEN_FLOAT_LITERAL)
//             floatLiteralCount++;

//         if (isKeyword(token.type))
//             keywordCount++;

//         if (isOperator(token.type))
//             operatorCount++;

//     } while (token.type != TOKEN_EOF);

//     fclose(fp);

//     printf("\n----- TOKEN STATISTICS -----\n");
//     printf("Total Tokens      : %d\n", totalTokens);
//     printf("Identifiers       : %d\n", identifierCount);
//     printf("Keywords          : %d\n", keywordCount);
//     printf("Int Literals      : %d\n", intLiteralCount);
//     printf("Float Literals    : %d\n", floatLiteralCount);
//     printf("Operators         : %d\n", operatorCount);

//     return 0;
// }


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include "target.h"

/* ─── helper: classify tokens ─── */
static int isKeyword(TokenType t) {
    return t==TOKEN_INT||t==TOKEN_FLOAT||t==TOKEN_IF||t==TOKEN_ELSE||
           t==TOKEN_WHILE||t==TOKEN_RETURN||t==TOKEN_PRINT||t==TOKEN_MAIN;
}
static int isOperator(TokenType t) {
    return t==TOKEN_PLUS||t==TOKEN_MINUS||t==TOKEN_MUL||t==TOKEN_DIV||
           t==TOKEN_ASSIGN||t==TOKEN_EQ||t==TOKEN_NEQ||
           t==TOKEN_LT||t==TOKEN_GT||t==TOKEN_LE||t==TOKEN_GE;
}

// static void banner(const char *title) {
//     printf("\n");
//     printf("╔══════════════════════════════════════════════════════╗\n");
//     printf("║  %-52s║\n", title);
//     printf("╚══════════════════════════════════════════════════════╝\n");
// }

static void banner(const char *title) {
    printf("\n");
    printf("===========================================================\n");
    printf("  %s\n", title);
    printf("===========================================================\n");
}

/* ════════════════════════════════════════════════════════════
   PHASE 1 — Lexical Analysis
   ════════════════════════════════════════════════════════════ */
static void runLexer(const char *filename) {
    banner("PHASE 1 : Lexical Analysis (Tokenization)");

    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("Cannot open input file"); exit(1); }

    Token tok;
    int total=0, ids=0, kws=0, ops=0, ints=0, floats=0;

    printf("\n  Line   Token Type              Lexeme\n");
    printf("  ─────  ──────────────────────  ──────────\n");

    do {
        tok = getNextToken(fp);
        printf("  %-5d  %-22s  %s\n",
               tok.line, tokenTypeToString(tok.type), tok.lexeme);
        if (tok.type != TOKEN_EOF) total++;
        if (tok.type == TOKEN_IDENTIFIER)   ids++;
        if (tok.type == TOKEN_INT_LITERAL)  ints++;
        if (tok.type == TOKEN_FLOAT_LITERAL)floats++;
        if (isKeyword(tok.type))            kws++;
        if (isOperator(tok.type))           ops++;
    } while (tok.type != TOKEN_EOF);

    fclose(fp);

    printf("  -- Token Statistics --\n");
    printf("  Total Tokens   : %d\n", total);
    printf("  Keywords       : %d\n", kws);
    printf("  Identifiers    : %d\n", ids);
    printf("  Int Literals   : %d\n", ints);
    printf("  Float Literals : %d\n", floats);
    printf("  Operators      : %d\n", ops);
}

/* ════════════════════════════════════════════════════════════
   PHASE 2 — Syntax Analysis (Parsing)
   ════════════════════════════════════════════════════════════ */
static ASTNode *runParser(const char *filename) {
    banner("PHASE 2 : Syntax Analysis (Parsing → AST)");

    /* Reset the line counter from lexer.c */
    extern int currentLine;
    currentLine = 1;

    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("Cannot open input file"); exit(1); }

    Parser  *p    = createParser(fp);
    ASTNode *root = parse(p);

    printf("\n  Abstract Syntax Tree:\n\n");
    printAST(root, 2);

    if (p->errorCount == 0)
        printf("\n  [OK] Parsing succeeded — no syntax errors.\n");
    else
        printf("\n  [!!] Parsing finished with %d error(s).\n", p->errorCount);

    fclose(fp);
    free(p);
    return root;
}

/* ════════════════════════════════════════════════════════════
   PHASE 3 — Semantic Analysis
   ════════════════════════════════════════════════════════════ */
static void runSemantic(ASTNode *root) {
    banner("PHASE 3 : Semantic Analysis");

    SemanticAnalyser *sa = createSemanticAnalyser();
    printf("\n  Symbol Table (declarations found):\n\n");
    analyseProgram(sa, root);

    printf("\n  ── Semantic Summary ──\n");
    printf("  Errors   : %d\n", sa->errorCount);
    printf("  Warnings : %d\n", sa->warningCount);

    if (sa->errorCount == 0)
        printf("  [OK] Semantic analysis passed.\n");

    freeSemanticAnalyser(sa);
}

/* ════════════════════════════════════════════════════════════
   PHASE 4 — Intermediate Code Generation (TAC)
   ════════════════════════════════════════════════════════════ */
static CodeGen *runCodeGen(ASTNode *root) {
    banner("PHASE 4 : Intermediate Code Generation (Three-Address Code)");

    CodeGen *cg = createCodeGen();
    generateCode(cg, root);

    printf("\n");
    printTAC(cg);
    printf("\n  [OK] TAC generation complete.\n");
    return cg;
}

/* ════════════════════════════════════════════════════════════
   PHASE 5 — Code Optimization
   ════════════════════════════════════════════════════════════ */
static void runOptimizer(CodeGen *cg) {
    banner("PHASE 5 : Code Optimization");

    printf("\n  Running optimizations...\n\n");
    OptStats stats = optimizeTAC(cg);

    printf("\n  Optimized TAC:\n\n");
    printTAC(cg);

    printf("\n  ── Optimization Summary ──\n");
    printf("  Constants folded      : %d\n", stats.foldedConstants);
    printf("  Dead assignments elim : %d\n", stats.eliminatedDeadCode);
    printf("  [OK] Optimization complete.\n");
}

/* ════════════════════════════════════════════════════════════
   PHASE 6 — Target Code Generation (Assembly)
   ════════════════════════════════════════════════════════════ */
static void runTargetGen(CodeGen *cg, const char *asmFile) {
    banner("PHASE 6 : Target Code Generation (Assembly)");

    generateAssembly(cg, asmFile);
    printf("\n  Assembly written to: %s\n", asmFile);

    /* Print the file for inspection */
    FILE *f = fopen(asmFile, "r");
    if (f) {
        char line[256];
        printf("\n");
        while (fgets(line, sizeof(line), f))
            printf("  %s", line);
        fclose(f);
    }
    printf("\n  [OK] Target code generation complete.\n");
}

/* ════════════════════════════════════════════════════════════
   Entry Point
   ════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {

    system("chcp 65001 > nul");
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    const char *srcFile = argv[1];

    /* Build output assembly filename (replace extension with .asm) */
    char asmFile[256];
    strncpy(asmFile, srcFile, sizeof(asmFile) - 5);
    char *dot = strrchr(asmFile, '.');
    if (dot) *dot = '\0';
    strcat(asmFile, ".asm");

    // printf("\n");
    // printf("  ███╗   ███╗██╗███╗   ██╗██╗      ██████╗\n");
    // printf("  ████╗ ████║██║████╗  ██║██║     ██╔════╝\n");
    // printf("  ██╔████╔██║██║██╔██╗ ██║██║     ██║     \n");
    // printf("  ██║╚██╔╝██║██║██║╚██╗██║██║     ██║     \n");
    // printf("  ██║ ╚═╝ ██║██║██║ ╚████║██║     ╚██████╗\n");
    // printf("  ╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝      ╚═════╝\n");
    // printf("        Mini-C Compiler  —  All 6 Phases\n");
    // printf("  Source: %s\n", srcFile);

printf("\n");
printf("  ==========================================\n");
printf("      MINI-C COMPILER  -  All 6 Phases      \n");
printf("  ==========================================\n");
printf("  Source : %s\n", srcFile);
printf("  Output : %s\n", asmFile);
printf("  ==========================================\n");

    /* ── Run all phases ── */
    runLexer(srcFile);

    ASTNode *root = runParser(srcFile);

    FILE *dotFile = fopen("ast.dot", "w");
    if (!dotFile) {
    perror("Error creating DOT file");
    exit(1);
    }

    fprintf(dotFile, "digraph AST {\n");
    fprintf(dotFile, "node [shape=box];\n");

    generateDOT(root, dotFile);

    fprintf(dotFile, "}\n");
    fclose(dotFile);

    runSemantic(root);

    CodeGen *cg = runCodeGen(root);

    runOptimizer(cg);

    runTargetGen(cg, asmFile);

    /* ── Cleanup ── */
    freeAST(root);
    freeCodeGen(cg);

    banner("Compilation Finished Successfully");
    printf("\n");
    return 0;
}

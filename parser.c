#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"

/* ─────────────────────────────────────────────
   Internal helpers
   ───────────────────────────────────────────── */

static ASTNode *newNode(NodeType type, const char *value, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = type;
    strncpy(n->value, value ? value : "", sizeof(n->value) - 1);
    n->line = line;
    return n;
}

static void advance(Parser *p) {
    p->current = getNextToken(p->fp);
}

static int check(Parser *p, TokenType t) {
    return p->current.type == t;
}

static int match(Parser *p, TokenType t) {
    if (check(p, t)) { advance(p); return 1; }
    return 0;
}

static void expect(Parser *p, TokenType t, const char *msg) {
    if (p->current.type == t) {
        advance(p);
    } else {
        fprintf(stderr,
            "[Parser Error] Line %d: Expected %s but got '%s'\n",
            p->current.line, msg, p->current.lexeme);
        p->errorCount++;
    }
}

/* ─────────────────────────────────────────────
   Forward declarations
   ───────────────────────────────────────────── */
static ASTNode *parseStatement(Parser *p);
static ASTNode *parseExpr(Parser *p);
static ASTNode *parseBlock(Parser *p);

/* ─────────────────────────────────────────────
   Expression parsing  (precedence climbing)
   ───────────────────────────────────────────── */

static ASTNode *parsePrimary(Parser *p) {
    Token tok = p->current;

    if (tok.type == TOKEN_INT_LITERAL) {
        advance(p);
        return newNode(NODE_INT_LITERAL, tok.lexeme, tok.line);
    }
    if (tok.type == TOKEN_FLOAT_LITERAL) {
        advance(p);
        return newNode(NODE_FLOAT_LITERAL, tok.lexeme, tok.line);
    }
    if (tok.type == TOKEN_IDENTIFIER) {
        advance(p);
        ASTNode *n = newNode(NODE_IDENTIFIER, tok.lexeme, tok.line);
        return n;
    }
    if (tok.type == TOKEN_LPAREN) {
        advance(p);
        ASTNode *inner = parseExpr(p);
        expect(p, TOKEN_RPAREN, "')'");
        return inner;
    }
    if (tok.type == TOKEN_MINUS) {
        advance(p);
        ASTNode *n = newNode(NODE_UNOP, "-", tok.line);
        n->left = parsePrimary(p);
        return n;
    }

    fprintf(stderr,
        "[Parser Error] Line %d: Unexpected token '%s' in expression\n",
        tok.line, tok.lexeme);
    p->errorCount++;
    advance(p);
    return newNode(NODE_INT_LITERAL, "0", tok.line);
}

static ASTNode *parseMulDiv(Parser *p) {
    ASTNode *left = parsePrimary(p);
    while (check(p, TOKEN_MUL) || check(p, TOKEN_DIV)) {
        Token op = p->current;
        advance(p);
        ASTNode *node = newNode(NODE_BINOP, op.lexeme, op.line);
        node->left  = left;
        node->right = parsePrimary(p);
        left = node;
    }
    return left;
}

static ASTNode *parseAddSub(Parser *p) {
    ASTNode *left = parseMulDiv(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        Token op = p->current;
        advance(p);
        ASTNode *node = newNode(NODE_BINOP, op.lexeme, op.line);
        node->left  = left;
        node->right = parseMulDiv(p);
        left = node;
    }
    return left;
}

static ASTNode *parseRelational(Parser *p) {
    ASTNode *left = parseAddSub(p);
    while (check(p, TOKEN_LT) || check(p, TOKEN_GT) ||
           check(p, TOKEN_LE) || check(p, TOKEN_GE)) {
        Token op = p->current;
        advance(p);
        ASTNode *node = newNode(NODE_BINOP, op.lexeme, op.line);
        node->left  = left;
        node->right = parseAddSub(p);
        left = node;
    }
    return left;
}

static ASTNode *parseEquality(Parser *p) {
    ASTNode *left = parseRelational(p);
    while (check(p, TOKEN_EQ) || check(p, TOKEN_NEQ)) {
        Token op = p->current;
        advance(p);
        ASTNode *node = newNode(NODE_BINOP, op.lexeme, op.line);
        node->left  = left;
        node->right = parseRelational(p);
        left = node;
    }
    return left;
}

static ASTNode *parseExpr(Parser *p) {
    return parseEquality(p);
}

/* ─────────────────────────────────────────────
   Statement parsing
   ───────────────────────────────────────────── */

static ASTNode *parseVarDecl(Parser *p) {
    /* current token is TOKEN_INT or TOKEN_FLOAT */
    int line = p->current.line;
    char typeName[20];
    strcpy(typeName, p->current.lexeme);
    advance(p);  /* consume type */

    char varName[100];
    if (!check(p, TOKEN_IDENTIFIER)) {
        fprintf(stderr,
            "[Parser Error] Line %d: Expected identifier after type '%s'\n",
            p->current.line, typeName);
        p->errorCount++;
        return NULL;
    }
    strcpy(varName, p->current.lexeme);
    advance(p);  /* consume name */

    ASTNode *decl = newNode(NODE_VAR_DECL, varName, line);
    strncpy(decl->value, varName, sizeof(decl->value) - 1);

    /* optional initialiser */
    if (match(p, TOKEN_ASSIGN)) {
        decl->left = parseExpr(p);
    }
    expect(p, TOKEN_SEMICOLON, "';'");
    return decl;
}

static ASTNode *parseIf(Parser *p) {
    int line = p->current.line;
    advance(p);  /* consume 'if' */
    expect(p, TOKEN_LPAREN, "'('");
    ASTNode *cond = parseExpr(p);
    expect(p, TOKEN_RPAREN, "')'");

    ASTNode *node = newNode(NODE_IF, "if", line);
    node->condition  = cond;
    node->thenBranch = parseBlock(p);

    if (check(p, TOKEN_ELSE)) {
        advance(p);
        if (check(p, TOKEN_IF))
            node->elseBranch = parseIf(p);
        else
            node->elseBranch = parseBlock(p);
    }
    return node;
}

static ASTNode *parseWhile(Parser *p) {
    int line = p->current.line;
    advance(p);  /* consume 'while' */
    expect(p, TOKEN_LPAREN, "'('");
    ASTNode *cond = parseExpr(p);
    expect(p, TOKEN_RPAREN, "')'");

    ASTNode *node = newNode(NODE_WHILE, "while", line);
    node->condition = cond;
    node->body      = parseBlock(p);
    return node;
}

static ASTNode *parseReturn(Parser *p) {
    int line = p->current.line;
    advance(p);  /* consume 'return' */
    ASTNode *node = newNode(NODE_RETURN, "return", line);
    if (!check(p, TOKEN_SEMICOLON))
        node->left = parseExpr(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    return node;
}

static ASTNode *parsePrint(Parser *p) {
    int line = p->current.line;
    advance(p);  /* consume 'print' */
    expect(p, TOKEN_LPAREN, "'('");
    ASTNode *node = newNode(NODE_PRINT, "print", line);
    node->left = parseExpr(p);
    expect(p, TOKEN_RPAREN, "')'");
    expect(p, TOKEN_SEMICOLON, "';'");
    return node;
}

static ASTNode *parseAssignOrExpr(Parser *p) {
    /* identifier '=' expr ';'  OR  expr ';'  */
    int line = p->current.line;
    if (check(p, TOKEN_IDENTIFIER)) {
        char name[100];
        strcpy(name, p->current.lexeme);
        advance(p);
        if (check(p, TOKEN_ASSIGN)) {
            advance(p);
            ASTNode *node = newNode(NODE_ASSIGN, name, line);
            node->left  = newNode(NODE_IDENTIFIER, name, line);
            node->right = parseExpr(p);
            expect(p, TOKEN_SEMICOLON, "';'");
            return node;
        }
        /* Not an assignment — put identifier back by building expression */
        /* (simple: we just treat leftover as expression statement)       */
        fprintf(stderr,
            "[Parser Error] Line %d: Expected '=' after identifier '%s'\n",
            line, name);
        p->errorCount++;
        expect(p, TOKEN_SEMICOLON, "';'");
        return newNode(NODE_IDENTIFIER, name, line);
    }
    /* generic expression statement */
    ASTNode *expr = parseExpr(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    return expr;
}

static ASTNode *parseStatement(Parser *p) {
    switch (p->current.type) {
        case TOKEN_INT:
        case TOKEN_FLOAT:   return parseVarDecl(p);
        case TOKEN_IF:      return parseIf(p);
        case TOKEN_WHILE:   return parseWhile(p);
        case TOKEN_RETURN:  return parseReturn(p);
        case TOKEN_PRINT:   return parsePrint(p);
        default:            return parseAssignOrExpr(p);
    }
}

static ASTNode *parseBlock(Parser *p) {
    int line = p->current.line;
    expect(p, TOKEN_LBRACE, "'{'");
    ASTNode *block = newNode(NODE_BLOCK, "block", line);

    ASTNode *head = NULL, *tail = NULL;
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        ASTNode *stmt = parseStatement(p);
        if (!stmt) continue;
        if (!head) { head = tail = stmt; }
        else        { tail->next = stmt; tail = stmt; }
    }
    block->body = head;
    expect(p, TOKEN_RBRACE, "'}'");
    return block;
}

/* ─────────────────────────────────────────────
   Top-level: int main() { ... }
   ───────────────────────────────────────────── */

static ASTNode *parseFunction(Parser *p) {
    int line = p->current.line;
    /* consume return type */
    if (!check(p, TOKEN_INT) && !check(p, TOKEN_FLOAT)) {
        fprintf(stderr,
            "[Parser Error] Line %d: Expected return type\n", line);
        p->errorCount++;
    } else {
        advance(p);
    }
    char fname[100] = "main";
    if (check(p, TOKEN_MAIN) || check(p, TOKEN_IDENTIFIER)) {
        strcpy(fname, p->current.lexeme);
        advance(p);
    }
    ASTNode *fn = newNode(NODE_FUNCTION, fname, line);
    expect(p, TOKEN_LPAREN, "'('");
    expect(p, TOKEN_RPAREN, "')'");
    fn->body = parseBlock(p);
    return fn;
}

/* ─────────────────────────────────────────────
   Public API
   ───────────────────────────────────────────── */

Parser *createParser(FILE *fp) {
    Parser *p = calloc(1, sizeof(Parser));
    p->fp = fp;
    advance(p);   /* load first token */
    return p;
}

ASTNode *parse(Parser *p) {
    ASTNode *root = newNode(NODE_PROGRAM, "program", 0);
    root->body = parseFunction(p);
    if (!check(p, TOKEN_EOF)) {
        fprintf(stderr,
            "[Parser Error] Line %d: Unexpected tokens after end of main\n",
            p->current.line);
        p->errorCount++;
    }
    return root;
}

/* ─────────────────────────────────────────────
   AST pretty-printer
   ───────────────────────────────────────────── */

static const char *nodeTypeName(NodeType t) {
    switch (t) {
        case NODE_PROGRAM:      return "PROGRAM";
        case NODE_FUNCTION:     return "FUNCTION";
        case NODE_BLOCK:        return "BLOCK";
        case NODE_VAR_DECL:     return "VAR_DECL";
        case NODE_ASSIGN:       return "ASSIGN";
        case NODE_IF:           return "IF";
        case NODE_WHILE:        return "WHILE";
        case NODE_RETURN:       return "RETURN";
        case NODE_PRINT:        return "PRINT";
        case NODE_BINOP:        return "BINOP";
        case NODE_UNOP:         return "UNOP";
        case NODE_INT_LITERAL:  return "INT_LIT";
        case NODE_FLOAT_LITERAL:return "FLOAT_LIT";
        case NODE_IDENTIFIER:   return "IDENTIFIER";
        case NODE_CALL:         return "CALL";
        default:                return "UNKNOWN";
    }
}

void printAST(ASTNode *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("[%s] \"%s\"  (line %d)\n",
           nodeTypeName(node->type), node->value, node->line);

    printAST(node->condition,  depth + 1);
    printAST(node->left,       depth + 1);
    printAST(node->right,      depth + 1);
    printAST(node->thenBranch, depth + 1);
    printAST(node->elseBranch, depth + 1);

    /* body/block: iterate sibling list */
    if (node->body) {
        for (int i = 0; i < depth + 1; i++) printf("  ");
        printf("{ body }\n");
        ASTNode *s = node->body;
        while (s) { printAST(s, depth + 2); s = s->next; }
    }
}

void freeAST(ASTNode *node) {
    if (!node) return;
    freeAST(node->left);
    freeAST(node->right);
    freeAST(node->condition);
    freeAST(node->thenBranch);
    freeAST(node->elseBranch);
    ASTNode *s = node->body;
    while (s) { ASTNode *nx = s->next; freeAST(s); s = nx; }
    free(node);
}

/* ─────────────────────────────────────────────
   Graphviz DOT generator
   ───────────────────────────────────────────── */

void generateDOT(ASTNode *node, FILE *fp) {
    if (!node) return;

    fprintf(fp, "node%p[label=\"%s\\n%s\"];\n",
            node, nodeTypeName(node->type), node->value);

    if (node->condition) {
        fprintf(fp, "node%p -> node%p [label=\"cond\"];\n", node, node->condition);
        generateDOT(node->condition, fp);
    }
    if (node->left) {
        fprintf(fp, "node%p -> node%p [label=\"left\"];\n", node, node->left);
        generateDOT(node->left, fp);
    }
    if (node->right) {
        fprintf(fp, "node%p -> node%p [label=\"right\"];\n", node, node->right);
        generateDOT(node->right, fp);
    }
    if (node->thenBranch) {
        fprintf(fp, "node%p -> node%p [label=\"then\"];\n", node, node->thenBranch);
        generateDOT(node->thenBranch, fp);
    }
    if (node->elseBranch) {
        fprintf(fp, "node%p -> node%p [label=\"else\"];\n", node, node->elseBranch);
        generateDOT(node->elseBranch, fp);
    }

    ASTNode *s = node->body;
    while (s) {
        fprintf(fp, "node%p -> node%p [label=\"body\"];\n", node, s);
        generateDOT(s, fp);
        s = s->next;
    }
}
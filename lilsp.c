#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "include/mpc.h"

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

// Forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* lval types */
enum { LVAL_LINT, LVAL_DEC, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

/* list of builtin function names */
#define MAX_BUILTINS 50
#define BUILTIN_NAME_LENGTH 10
char builtins[MAX_BUILTINS][BUILTIN_NAME_LENGTH];

char* ltype_name(int typeName) {
    switch(typeName) {
        case LVAL_FUN: return "Function";
        case LVAL_LINT: return "Integer";
        case LVAL_DEC: return "Decimal";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

typedef lval*(*lbuiltin)(lenv*, lval*);

/* lilsp value struct */
struct lval {
    int type;
    long lint;
    double dec;
    char* err;
    char* sym;
    lbuiltin fun;
    /* Count and pointer to list of lvals */
    int count;
    struct lval** cell;
};

/* define environment strucutre */
struct lenv {
    int count;
    char** syms;
    lval** vals;
};

/* Create new pointer to integer type */
lval* lval_lint(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_LINT;
    v->lint = x;
    return v;
}

/* Create new pointer to decimal type */
lval* lval_dec(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DEC;
    v->dec = x;
    return v;
}

/* Create pointer to error type */
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    // create va list and initialize
    va_list va;
    va_start(va, fmt);

    // allocate 512 bytes
    v->err = malloc(512);
    // printf error (max 511 chars)
    vsnprintf(v->err, 511, fmt, va);
    //reallocate number of bytes used
    v->err = realloc(v->err, strlen(v->err)+1);
    va_end(va);

    return v;
}

/* Create pointer to symbol type */
lval* lval_sym(char* symbolName) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(symbolName) + 1);
    strcpy(v->sym, symbolName);
    return v;
}

/* Create pointer to new sexpr type */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create pointer to new Qexpr type */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* create pointer to function type */
lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

/* create pointer to new lenv type */
lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

// Reallocate memory and add item to list of children
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

/* Delete an lval and free it's children's memory (if applicable) */
void lval_del(lval* v) {
    switch (v->type) {
        // nothing special for numbers
        case LVAL_LINT:
        case LVAL_DEC: break;

        // For err and symbol, release string data
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // Delete all elements within sexpr or qexpr
        case LVAL_QEXPR:
        case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        case LVAL_FUN: break;
        // Free memory allocated to hold pointers
        free(v->cell);
        break;
    }
    free(v);
}

/* delete an environment */
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/* copy lval */
lval* lval_copy(lval* v) {
    lval*x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_DEC:
            x->dec = v->dec;
            break;
        case LVAL_LINT:
            x->lint = v->lint;
            break;

        // Copy strings
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        
        // Copy lists recursively
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

/* get an item from the environment */
lval* lenv_get(lenv* e, lval* k) {
    // Iterate over all items
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("Unbound symbol '%s'", k->sym);
}

/* assign a symbol to an expression */
void lenv_put(lenv* e, lval* k, lval* v) {
    // check if variable already exists
    for (int i = 0; i < e->count; i++) {
        // If variable already exists delete and replace with new value
        if (strcmp(e->syms[i], k->sym) ==0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // Allocate space for new entry
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    // copy contents of lval and symbol to new location
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

// Forward declare
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value
        lval_print(v->cell[i]);

        // If last element don't print trailing space
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* print an lval type */
void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_LINT:
            printf("%li", v->lint);
            break;
        case LVAL_DEC:
            printf("%f", v->dec);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUN:
            printf("<function>");
            break;
    }
}

void lval_println(lval *v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    if (strstr(t->tag, "integer")) {
        // Check for conversion errors
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_lint(x) : lval_err("Invalid number '%s'", t->contents);
    }
    // Decimal type
    if (strstr(t->tag, "decimal")) {
        // Check for conversion errors
        errno = 0;
        float x = strtof(t->contents, NULL);
        return errno != ERANGE ? lval_dec(x) : lval_err("Invalid number '%s'", t->contents);
    }

    return lval_err("Invalid number '%s'", t->contents);
}

lval* lval_read(mpc_ast_t* t) {
    // Return symbol or number
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // If root or sexpr, create an empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    // Fill list with valid expressions
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }

        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));        
    }
    return x;
}

// Pop item from list of lvals: remove it and shuffle other elements up
lval* lval_pop(lval* v, int i) {
    // get item at i
    lval* x = v->cell[i];

    // Shift memory after item
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count-i-1));

    // Decrease count in lval
    v->count--;

    // Reallocate memory
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

// Pop lval then delete remaining list
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

// Define functionality for builtin operators
lval* builtin_op(lenv* e, lval* a, char* op) {
    // Ensure all args are numbers
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, (a->cell[i]->type == LVAL_LINT || a->cell[i]->type == LVAL_DEC), "Cannot apply operator '%s' to argument of type %s. Argument must be a numeric type.", op, ltype_name(a->cell[i]->type));
   }

    // Pop the first element
    lval* x = lval_pop(a, 0);

    // unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        if (x->type == LVAL_LINT) {
            x->lint = -x->lint;
        } else {
            x->dec = -x->dec;
        }
    }

    // While there are elements remaining
    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (x->type != y->type) {
            lval_del(x);
            lval_del(y);
            x = lval_err("Numeric types don't match.");
            break;
        }

        if (x->type == LVAL_LINT) {
            if (strcmp(op, "+") == 0) { x->lint += y->lint; }
            if (strcmp(op, "-") == 0) { x->lint -= y->lint; }
            if (strcmp(op, "*") == 0) { x->lint *= y->lint; }
            if (strcmp(op, "/") == 0) { 
                if (y->lint == 0) {
                   lval_del(x);
                   lval_del(y);
                   x = lval_err("Division by zero");
                   break; 
                }
                x->lint /= y->lint;
            }
            if (strcmp(op, "%") == 0) { 
                if (y->lint == 0) {
                   lval_del(x);
                   lval_del(y);
                   x = lval_err("Division by zero");
                   break; 
                }
                x->lint %= y->lint;
            }
            lval_del(y);
        }

        if (x->type == LVAL_DEC) {
            if (strcmp(op, "+") == 0) { x->dec += y->dec; }
            if (strcmp(op, "-") == 0) { x->dec -= y->dec; }
            if (strcmp(op, "*") == 0) { x->dec *= y->dec; }
            if (strcmp(op, "/") == 0) { 
                if (y->dec == 0) {
                   lval_del(x);
                   lval_del(y);
                   x = lval_err("Division by zero");
                   break; 
                }
                x->dec /= y->dec;
            }
            if (strcmp(op, "%") == 0) { 
                if (y->dec == 0) {
                   lval_del(x);
                   lval_del(y);
                   x = lval_err("Division by zero");
                   break; 
                }
                x->dec = fmod(x->dec, y->dec);
            }
            lval_del(y);
        }
    }
    lval_del(a);
    return x;
}

/* Builtin operators */
lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

// Forward definition
lval* lval_eval_sexpr(lval* v, lenv* e);

lval* lval_eval(lenv* e, lval* v) {
    // Get symbol and delete
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    // Evaluate S-Expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v, e); }
    return v;
}

lval* lval_eval_sexpr(lval* v, lenv* e) {
    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // Empty expressions
    if (v->count == 0) { return v; }

    // Single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // Ensure first element is a function
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("First element is not a function.");
    }

    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

/* get the head of a Qexpr */
lval* builtin_head(lenv* e, lval* a) {
    // check error conditions
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments. "
    "Got %i, expected %i.",
    a->count, 1); 

    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type for argument 1."
    "Got %s, expected %s.",
    ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed nothing.")

    // Otherwise, take first argument
    lval* v = lval_take(a, 0);

    // Delete all non-head elements then return
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/* get the tail of a Qexpr */
lval* builtin_tail(lenv* e, lval* a) {
    // check errors
    LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments."
    "Got %i, expected %i.",
    a->count, 1); 

    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect type for argument 1. "
    "Got %s, expected %s.",
    ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed nothing.");

    // Take first arg, dispose and return
    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

/* convert S-Expression to Qexpr and return */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* eval a Q-expr by converting to s-expr */
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments."
    "Got %i, expected %i.",
    a->count, 1); 

    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type for argument 1. "
    "Got %s expected %s.",
    ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lenv* e, lval* x, lval* y) {
    // add each cell in y to x
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    // Delete empty y, return x
    lval_del(y);
    return x;
}

/* join Q-Exprs */
lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,"Function 'join' passed incorrect type for argument %i. "
        "Got %s, expected %s",
        i+1, ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR));
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x=lval_join(e, x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

/* function definition */
lval* builtin_def(lenv* e, lval* a) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'def' passed incorrect type for argument 1. "
    "Got %s, expected %s",
    ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    // First arg is list of symbols
    lval* syms = a->cell[0];

    // Ensure all elements of list are symbols
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' expected a symbol at argument %i, instead got %s.", i+1, ltype_name(syms->cell[i]->type));
    }
    // Check correct number of symbols to values
    LASSERT(a, syms->count == a->count-1, "Incorrect number of values passed. "
    "Expected %i, got %i.",
    syms->count, a->count-1);

    int len = sizeof(builtins)/sizeof(builtins[0]);

    // Assign *copies* of values to symbols
    for (int i = 0; i < syms->count; i++) {
        // Check symbol is not already a builtin
        for (int i = 0; i < len; i++) {
            LASSERT(a, !strcmp(syms->cell[i]->, builtins[i]), "Cannot redefine builtin function '%s'.", builtins[i]);
        }
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

/* add builtins to environment */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    for (int i = 0; i < MAX_BUILTINS; i++) {
        if (strcmp(builtins[i]))
    }
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    // List funcs
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "def", builtin_def);

    // Math functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_head);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
}



int main(int argc, char** argv) {
    /* Define RPN grammar */
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lilsp = mpc_new("lilsp");

    /* Define with following language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                                   \
            integer     : /-?[0-9]+/ ;                                                      \
            decimal     : /-?[0-9]+\\.[0-9]+/ ;                                             \
            number      : <decimal> | <integer> ;                                           \
            symbol      : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                                \
            sexpr       : '(' <expr>* ')' ;                                                 \
            qexpr       : '{' <expr>* '}' ;                                                 \
            expr        : <number> | <symbol> | <sexpr> | <qexpr> ;                         \
            lilsp       : /^/ <expr>* /$/ ;                                                 \
        ",
        Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lilsp);

    puts("Lilsp Version 0.0.0.1");
    puts("Press Ctrl+C to exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    // REPL
    while(1) {
        // output prompt and get input
        char* input = readline("lilsp> ");
        
        add_history(input);

        // Parse user input
        mpc_result_t r;
        // mpc_parse will parse input according to grammar then copy result into r.
        // return 1 on success, 0 on failure
        if (mpc_parse("<stdin>", input, Lilsp, &r)) {
            // Print result of evaluation
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);

            // Clean up ast from memory
            mpc_ast_delete(r.output);

        } else {
            // Print error on failure and clean up
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_del(e);

    // Clean up parsers
    mpc_cleanup(8, Integer, Decimal, Number, Symbol, Sexpr, Qexpr, Expr, Lilsp);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

/* lval types */
enum { LVAL_LINT, LVAL_DEC, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* lilsp value struct */
typedef struct lval {
    int type;
    long lint;
    double dec;
    char* err;
    char* sym;
    /* Count and pointer to list of lvals */
    int count;
    struct lval** cell;
} lval;

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
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Create pointer to symbol type */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
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

        // Delete all elements within sexpr
        case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        // Free memory allocated to hold pointers
        free(v->cell);
        break;
    }
    free(v);
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
        return errno != ERANGE ? lval_lint(x) : lval_err("Invalid number");
    }
    // Decimal type
    if (strstr(t->tag, "decimal")) {
        // Check for conversion errors
        errno = 0;
        float x = strtof(t->contents, NULL);
        return errno != ERANGE ? lval_dec(x) : lval_err("Invalid number");
    }

    return lval_err("Invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // Return symbol or number
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // If root or sexpr, create an empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

    // Fill list with valid expressions
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
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
lval* builtin_op(lval* a, char* op) {
    // Ensure all args are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_LINT && a->cell[i]->type != LVAL_DEC) {
            lval_del(a);
            return lval_err("Cannot apply operator to non-number");
        }
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

// Forward definition
lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v) {
    // Evaluate S-Expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}

lval* lval_eval_sexpr(lval* v) {
    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // Empty expressions
    if (v->count == 0) { return v; }

    // Single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // Ensure first element is a symbol
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-Expression cannot start with a symbol");
    }

    // Call builtin op
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

int main(int argc, char** argv) {
    /* Define RPN grammar */
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lilsp = mpc_new("lilsp");

    /* Define with following language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                                   \
            integer     : /-?[0-9]+/ ;                                                      \
            decimal     : /-?[0-9]+\\.[0-9]+/ ;                                             \
            number      : <decimal> | <integer> ;                                           \
            symbol      : '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" ;                 \
            sexpr       : '(' <expr>* ')' ;                                                 \
            expr        : <number> | <symbol> | <sexpr> ;                                   \
            lilsp       : /^/ <expr>* /$/ ;                                                 \
        ",
        Integer, Decimal, Number, Symbol, Sexpr, Expr, Lilsp);

    puts("Lilsp Version 0.0.0.1");
    puts("Press Ctrl+C to exit\n");

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
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);

            // Clean up ast from memory
            mpc_ast_delete(r.output);

        } else {
            // Print error on failure and clean up
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
    }

    // Clean up parsers
    mpc_cleanup(7, Integer, Decimal, Number, Symbol, Sexpr, Expr, Lilsp);
    return 0;
}
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

/*lval eval_op(lval x, char* op, lval y) {
    // If either value is an error, return it
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    // Different return types based on whether the operands are integers or floats
    //TODO: make this work with mixed types
    if (x.type == LVAL_LINT && y.type == LVAL_LINT) {
        if (strcmp(op, "+") == 0) { return lval_lint(x.lint + y.lint); }
        if (strcmp(op, "-") == 0) { return lval_lint(x.lint - y.lint); }
        if (strcmp(op, "*") == 0) { return lval_lint(x.lint * y.lint); }
        if (strcmp(op, "/") == 0) {
            return y.lint == 0 ? lval_err(LERR_DIV_ZERO) : lval_lint(x.lint / y.lint);
        }
        if (strcmp(op, "%") == 0) {
            return y.lint == 0 ? lval_err(LERR_DIV_ZERO) : lval_lint(x.lint % y.lint);
        }
        if (strcmp(op, "min") == 0) { return lval_lint(fmin(x.lint, y.lint)); }
        if (strcmp(op, "max") == 0) { return lval_lint(fmax(x.lint, y.lint)); }

        // Correct types but reached end without finding a matching operator
        return lval_err(LERR_BAD_OP);
    }

    if (x.type == LVAL_DEC && y.type == LVAL_DEC) {
        if (strcmp(op, "+") == 0) { return lval_dec(x.dec + y.dec); }
        if (strcmp(op, "-") == 0) { return lval_dec(x.dec - y.dec); }
        if (strcmp(op, "*") == 0) { return lval_dec(x.dec * y.dec); }
        if (strcmp(op, "/") == 0) {
            return y.dec == 0 ? lval_err(LERR_DIV_ZERO) : lval_dec(x.dec / y.dec);
        }
        if (strcmp(op, "%") == 0) {
            return y.dec == 0 ? lval_err(LERR_DIV_ZERO) : lval_dec(fmod(x.dec, y.dec));
        }
        if (strcmp(op, "min") == 0) { return lval_dec(fmin(x.dec, y.dec)); }
        if (strcmp(op, "max") == 0) { return lval_dec(fmax(x.dec, y.dec)); }

        return lval_err(LERR_BAD_OP);
    }

    // The types of the operands didn't match
    return lval_err(LERR_TYPE_MISMATCH);
}

int number_of_nodes(mpc_ast_t* t) {
    if (t->children_num == 0) {
        return 1;
    }
    if (t->children_num >= 1) {
        int total = 1;
        for (int i = 0; i < t->children_num; i++) {
            total = total + number_of_nodes(t->children[i]);
        }
        return total;
    }
    return 0;
}

lval eval(mpc_ast_t* t) {
    // Return numbers immediately
    if (strstr(t->tag, "number")) {
        // Int type
        if (strstr(t->tag, "integer")) {
            // Check for conversion errors
            errno = 0;
            long x = strtol(t->contents, NULL, 10);
            return errno != ERANGE ? lval_lint(x) : lval_err(LERR_BAD_NUM);
        }
        // Decimal type
        if (strstr(t->tag, "decimal")) {
            // Check for conversion errors
            errno = 0;
            float x = strtof(t->contents, NULL);
            return errno != ERANGE ? lval_dec(x) : lval_err(LERR_BAD_NUM);
        }

    }
    // Operator is second child (first is '(')
    char* op = t->children[1]->contents;

    // Store third child in x
    lval x = eval(t->children[2]);

    // Iterate over remaining children and evaluate each operation
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}*/

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
            lval* x = lval_read(r.output);
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
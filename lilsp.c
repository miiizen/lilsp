#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

/* lval types */
enum { LVAL_LINT, LVAL_DEC, LVAL_ERR };

/* Error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM, LERR_TYPE_MISMATCH };

/* lilsp value struct */
typedef struct {
    int type;
    long lint;
    double dec;
    int err;
} lval;

/* Create new integer type */
lval lval_lint(long x) {
    lval v;
    v.type = LVAL_LINT;
    v.lint = x;
    return v;
}

/* Create new decimal type */
lval lval_dec(double x) {
    lval v;
    v.type = LVAL_DEC;
    v.dec = x;
    return v;
}

/* create error type */
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

/* print an lval type */
void lval_print(lval v) {
    switch(v.type) {
        case LVAL_LINT:
            printf("%li", v.lint);
            break;
        case LVAL_DEC:
            printf("%f", v.dec);
            break;
        case LVAL_ERR:
            // Check error type and print
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division by zero");
            }
            if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid operator");
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid number");
            }
            if (v.err == LERR_TYPE_MISMATCH) {
                printf("Error: Mismatched types");
            }
            break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char* op, lval y) {
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
}

int main(int argc, char** argv) {
    /* Define RPN grammar */
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Decimal = mpc_new("decimal");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lilsp = mpc_new("lilsp");

    /* Define with following language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                                   \
            integer     : /-?[0-9]+/ ;                                                      \
            decimal     : /-?[0-9]+\\.[0-9]+/ ;                                             \
            number      : <decimal> | <integer> ;                                           \
            operator    : '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" ;                 \
            expr        : <number> | '(' <operator> <expr>+ ')' ;                           \
            lilsp       : /^/ <operator> <expr>+ /$/ ;                                      \
        ",
        Integer, Decimal, Number, Operator, Expr, Lilsp);

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
            lval result = eval(r.output);
            lval_println(result);

            // Clean up ast from memory
            mpc_ast_delete(r.output);

        } else {
            // Print error on failure and clean up
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
    }

    // Clean up parsers
    mpc_cleanup(4, Number, Operator, Expr, Lilsp);
    return 0;
}
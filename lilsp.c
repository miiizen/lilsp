#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    if (strcmp(op, "%") == 0) { return x % y; }
    if (strcmp(op, "min") == 0) { return fmin(x, y); }
    if (strcmp(op, "max") == 0) { return fmax(x, y); }


    return 0;
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

long eval(mpc_ast_t* t) {
    // Return numbers immediately
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }
    // Operator is second child (first is '(')
    char* op = t->children[1]->contents;

    // Store third child in x
    long x = eval(t->children[2]);

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
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lilsp = mpc_new("lilsp");

    /* Define with following language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                                   \
            number      : /-?[0-9]+/ ;                                                      \
            operator    : '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" ;                     \
            expr        : <number> | '(' <operator> <expr>+ ')' ;                           \
            lilsp        : /^/ <operator> <expr>+ /$/ ;                                     \
        ",
        Number, Operator, Expr, Lilsp);

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
            long result = eval(r.output);
            printf("%li\n", result);

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
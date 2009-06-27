#include <stdio.h>
#include <stdlib.h>

#include "forth.h"
#include "stack.h"

DictDebug junk;  // Make sure DictDebug symbol gets compiled in

Stack               parameter_stack;
Stack               return_stack;
Stack               control_stack;

InterpreterState    interpreter_state;
DocolonMode         docolon_mode;

Error               error_state;
char                error_message[MAX_ERROR_LEN];

jmp_buf             cold_boot;
jmp_buf             warm_boot;

extern DictEntry _dict_var_LATEST;  // This is "private" to builtin.c, but needed here to reinit

int main (int argc, char **argv) {

    // ABORT supposedly clears the data stack and then calls QUIT
    // QUIT supposedly clears the return stack and then starts interpreting
    // i don't have my own return stack

    // ABORT jumps to here
    setjmp(cold_boot); 

    interpreter_state = S_INTERPRET;
    docolon_mode = DM_NORMAL;
    stack_init(&parameter_stack);
    mem_init();
    *var_BASE = 0;
    *var_LATEST = (cell) &_dict_var_LATEST;

    // QUIT jumps to here
    setjmp(warm_boot);
    stack_init(&return_stack);

    switch (error_state) {
        // FIXME can this stuff be done with longjmp return value stuff?
        case E_OK: break;
        case E_PARSE:
            fprintf(stderr, "Parse error: %s\n", error_message);
            break;
    }

    error_state = E_OK;
    memset(error_message, 0, sizeof(error_message));

    // Run the interpreter
    while (1) {
        do_interpret(NULL);
    }

    exit(0);
}

#include <stdio.h>
#include <stdlib.h>

#include "forth.h"

#define F_IMMED     0x80
#define F_HIDDEN    0x20
#define F_LENMASK   0x1F

// Function signature for a primitive
#define DECLARE_PRIMITIVE(P)    void P (void *pfa)

// Define a primitive and add it to the dictionary
#define PRIMITIVE(NAME, NAME_LEN, FLAGS, CNAME, LINK)                               \
    DECLARE_PRIMITIVE(CNAME);                                                       \
    DictEntry _dict_##CNAME = { &_dict_##LINK, FLAGS | NAME_LEN, NAME, CNAME, };    \
    DECLARE_PRIMITIVE(CNAME)

// Define a variable and add it to the dictionar; also create a pointer for direct access
#define VARIABLE(NAME, NAME_LEN, INITIAL, LINK)                                             \
    DictEntry _dict_var_##NAME = { &_dict_##LINK, NAME_LEN, #NAME, do_variable, {INITIAL}}; \
    cell * const var_##NAME = &_dict_var_##NAME.param[0]

// Define a constant and add it to the dictionary
#define CONSTANT(NAME, NAME_LEN, VALUE, LINK) \
    DictEntry _dict_const_##NAME = { &_dict_##LINK, NAME_LEN, #NAME, do_constant, { VALUE }}

// Shorthand macros to make repetitive code less hassle
#define REG(__x)        register cell __x
#define PTOP(__x)       __x = stack_top(&parameter_stack)
#define PPOP(__x)       __x = stack_pop(&parameter_stack)
#define PPUSH(__x)      stack_push(&parameter_stack, (__x))





// FIXME do i want these all to take a void* argument, or should they just use a global register
// W to read the pfa from?

// I think they need a pointer for where they're coming from, so that eg LIT can read the next
// code word... but I need to think about this some more cause maybe i already have this info.


// all primitives are called with a parameter, which is a pointer to the parameter field
// of the word definition they're called from.
// for most primitives, this is simply ignored, as all their data comes from the stack
// for special primitives eg do_colon, do_constant etc, this is needed to get the inner contents
// of the word definition.


                      
/*
+------+-----+------+------+-------+-------+-----+
| link | len | name | code | param | param | ... |
+------+-----+------+-|----+-|-----+-|-----+-----+
           do_colon <-/      |       \-> (code field of next word...)
                             V
        +------+-----+------+------+-------+-------+-----+
        | link | len | name | code | param | param | ... |
        +------+-----+------+-|----+-|-----+-|-----+-----+
                   do_colon <-/      |       \-> (code field of next word...)
                                     V
                +------+-----+------+------+-------+-------+-----+
                | link | len | name | code | param | param | ... |
                +------+-----+------+-|----+-|-----+-|-----+-----+
                           do_colon <-/      |       \-> (code field of next word...)
                                             \-> (etc)

*/

void do_colon2(void *cfa) {
    // This version expects its argument to point to the CFA rather than the PFA
    for (pvf **code = (pvf **) cfa + 1; code && *code; code++) {
        (***code)(*code);
    }
}



/*
    pfa     = parameter field address, contains address of parameter field
    *pfa    = parameter field, contains address of code field for next word
    **pfa   = code field, contains address of function to process the word
    ***pfa  = interpreter function for the word 
 */
void do_colon(void *pfa) {

    // deref pfa to get the cfa of the word to execute
    // increment pfa and do it again
    // stop when pfa derefs to null

//    pvf **param = (pvf **) pfa; // W?
//    for (int i=0; param[i] != NULL; i++) {
//        W = param[i] + 1;
//        (*param[i]) ( param[i] + 1 );
//    }

    pvf **param;
    for (param = (pvf **) pfa; param && *param; param++) {
        pvf *code = *param;
        (**code) (code + 1);
    }

//    pvf **param = (pvf **) pfa; // W?
//    while (param && *param) {  // param = 168
//        pvf *code = *param;    // code = 292
//        void *arg = code + 1;  // arg = 296
//        W = arg;

//        (**code) ( arg );

//        param++;
//    }
}


/*
  finds a constant in the parameter field and pushes its VALUE onto the parameter stack
 */
void do_constant(void *pfa) {
    // pfa is a pointer to the parameter field, so deref it to get the constant
    PPUSH(*((cell *) pfa));
}

/*
  finds a variable in the parameter field and pushes its ADDRESS onto the parameter stack
*/
void do_variable(void *pfa) {
    // pfa is a pointer to the parameter field, so push its value (the address) as is
    PPUSH((cell) pfa);
}



/***************************************************************************
    The DICT_ROOT DictEntry delimits the root of the dictionary
 ***************************************************************************/
DictEntry _dict_DICT_ROOT = {
    NULL,   // link
    0,      // name length + flags
    "",     // name
    NULL,   // code
};


/***************************************************************************
  Builtin variables -- keep these together
  Exception: LATEST should be declared very last (see end of file)
    VARIABLE(NAME, NAME_LEN, INITIAL, LINK)
***************************************************************************/
VARIABLE (STATE, 5, 0, DICT_ROOT);    // FIXME idk
VARIABLE (BASE, 4, 0, var_STATE);  // FIXME idk


/***************************************************************************
  Builtin constants -- keep these together
    CONSTANT(NAME, NAME_LEN, VALUE, LINK)
 ***************************************************************************/
CONSTANT (VERSION, 7, 0, var_BASE);
CONSTANT (DOCOL, 5, (cell) &do_colon, const_VERSION);
CONSTANT (SHEEP, 5, 0xDEADBEEF, const_DOCOL);


/***************************************************************************
  Builtin code words -- keep these together
    PRIMITIVE(NAME, NAME_LEN, FLAGS, CNAME, LINK)
 ***************************************************************************/

// ( a -- )
PRIMITIVE ("DROP", 4, 0, _DROP, const_DOCOL) {
    REG(a);
    PPOP(a);
}


// ( b a -- a b )
PRIMITIVE ("SWAP", 4, 0, _SWAP, _DROP) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a);
    PPUSH(b);
}


// ( a - a a )
PRIMITIVE ("DUP", 3, 0, _DUP, _SWAP) {
    REG(a);
    
    PTOP(a);
    PPUSH(a);
}


// ( b a -- a b )
PRIMITIVE ("OVER", 4, 0, _OVER, _DUP) {
    REG(a);
    REG(b);

    PPOP(a);
    PTOP(b);
    PPUSH(a);
    PPUSH(b);
}


// ( c b a -- a c b )
PRIMITIVE ("ROT", 3, 0, _ROT, _OVER) {
    REG(a);
    REG(b);
    REG(c);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPUSH(a);
    PPUSH(c);
    PPUSH(b);
}


// ( c b a -- b a c )
PRIMITIVE ("-ROT", 4, 0, _negROT, _ROT) {
    REG(a);
    REG(b);
    REG(c);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPUSH(b);
    PPUSH(a);
    PPUSH(c);
}


// ( b a -- )
PRIMITIVE ("2DROP", 5, 0, _2DROP, _negROT) {
    REG(a);
    PPOP(a);
    PPOP(a);
}


// ( b a -- b a b a )
PRIMITIVE ("2DUP", 4, 0, _2DUP, _2DROP) {
    REG(a);
    REG(b);

    PPOP(a);
    PTOP(b);
    PPUSH(a);
    PPUSH(b);
    PPUSH(a);
}


// ( d c b a -- b a d c ) 
PRIMITIVE ("2SWAP", 5, 0, _2SWAP, _2DUP) {
    REG(a);
    REG(b);
    REG(c);
    REG(d);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPOP(d);

    PPUSH(b);
    PPUSH(a);
    PPUSH(d);
    PPUSH(c);
}


// ( a -- a ) or ( a -- a a )
PRIMITIVE ("?DUP", 4, 0, _qDUP, _2SWAP) {
    REG(a);

    PTOP(a);
    if (a)  PPUSH(a);
}


// ( a -- a+1 )
PRIMITIVE ("1+", 2, 0, _1plus, _qDUP) {
    REG(a);

    PPOP(a);
    PPUSH(a + 1);
}


// ( a -- a-1 )
PRIMITIVE ("1-", 2, 0, _1minus, _1plus) {
    REG(a);
    
    PPOP(a);
    PPUSH(a - 1);
}


// ( a -- a+4 )
PRIMITIVE ("4+", 2, 0, _4plus, _1minus) {
    REG(a);

    PPOP(a);
    PPUSH(a + 4);
}


// ( a -- a-4 )
PRIMITIVE ("4-", 2, 0, _4minus, _4plus) {
    REG(a);

    PPOP(a);
    PPUSH(a - 4);
}


// ( b a -- a+b )
PRIMITIVE ("+", 1, 0, _plus, _4minus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a + b);
}


// ( b a -- b - a )
PRIMITIVE ("-", 1, 0, _minus, _plus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b - a);
}


// ( b a -- a * b )
PRIMITIVE ("*", 1, 0, _multiply, _minus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a * b);
}


// ( b a -- b / a)
PRIMITIVE ("/", 1, 0, _divide, _multiply) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b / a);
}


// ( b a -- b % a)
PRIMITIVE ("MOD", 3, 0, _modulus, _divide) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b % a);
}


// ( b a -- a == b )
PRIMITIVE ("=", 1, 0, _equals, _modulus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(a == b);
}


// ( b a -- a != b )
PRIMITIVE ("<>", 2, 0, _notequals, _equals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(a != b);
}


// ( b a -- b < a )
PRIMITIVE ("<", 1, 0, _lt, _notequals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b < a);
}


// ( b a -- b > a )
PRIMITIVE (">", 1, 0, _gt, _lt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b > a);
}


// ( b a -- b <= a )
PRIMITIVE ("<=", 2, 0, _lte, _gt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b <= a);
}


// ( b a -- b >= a )
PRIMITIVE (">=", 2, 0, _gte, _lte) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b >= a);
}


// ( a -- a == 0 )
PRIMITIVE ("0=", 2, 0, _zero_equals, _gte) {
    REG(a);

    PPOP(a);
    PPUSH(a == 0);
}


// ( a -- a != 0 )
PRIMITIVE ("0<>", 3, 0, _notzero_equals, _zero_equals) {
    REG(a);

    PPOP(a);
    PPUSH(a != 0);
}


// ( a -- a < 0 )
PRIMITIVE ("0<", 2, 0, _zero_lt, _notzero_equals) {
    REG(a);

    PPOP(a);
    PPUSH(a < 0);
}


// ( a -- a > 0 )
PRIMITIVE ("0>", 2, 0, _zero_gt, _zero_lt) {
    REG(a);

    PPOP(a);
    PPUSH(a > 0);
}


// ( a -- a <= 0 )
PRIMITIVE ("0<=", 3, 0, _zero_lte, _zero_gt) {
    REG(a);

    PPOP(a);
    PPUSH(a <= 0);
}


// ( a -- a >= 0 )
PRIMITIVE ("0>=", 3, 0, _zero_gte, _zero_lte) {
    REG(a);

    PPOP(a);
    PPUSH(a >= 0);
}


// ( b a -- a & b )
PRIMITIVE ("AND", 3, 0, _AND, _zero_gte) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a & b);
}


// ( b a -- a | b )
PRIMITIVE ("OR", 2, 0, _OR, _AND) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a | b);
}


// ( b a -- a ^ b )
PRIMITIVE ("XOR", 3, 0, _XOR, _OR) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a ^ b);
}


// ( a -- ~a )
PRIMITIVE ("INVERT", 6, 0, _INVERT, _XOR) {
    REG(a);

    PPOP(a);
    PPUSH(~a);
}


/* Memory access primitives */

// ( value addr -- )
PRIMITIVE ("!", 1, 0, _store, _INVERT) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);

    *((cell *) a) = b;
}


// ( addr -- value )
PRIMITIVE ("@", 1, 0, _fetch, _store) {
    REG(a);
    REG(b);

    PPOP(a);
    b = *((cell *) a);
    PPUSH(b);
}


// ( delta addr -- )
PRIMITIVE ("+!", 2, 0, _addstore, _fetch) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((cell*) a) += b;
}


// ( delta addr -- )
PRIMITIVE ("-!", 2, 0, _substore, _addstore) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((cell*) a) -= b;
}


// ( value addr -- )
PRIMITIVE ("C!", 2, 0, _storebyte, _substore) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((char*) a) = (char) (b & 0xFF);
}


// ( addr -- value )
PRIMITIVE ("C@", 2, 0, _fetchbyte, _storebyte) {
    REG(a);
    REG(b);

    PPOP(a);
    b = *((char*) a);
    PPUSH(b);
}


// ( FIXME )
PRIMITIVE ("C@C!", 4, 0, _ccopy, _fetchbyte) {
    // FIXME wtf
}


// ( FIXME )
PRIMITIVE ("CMOVE", 5, 0, _cmove, _ccopy) {
    // FIXME wtf
}


// ( -- char )
PRIMITIVE ("KEY", 3, 0, _KEY, _cmove) {
    REG(a);

    a = fgetc(stdin);
    PPUSH(a);
}


// ( char -- )
PRIMITIVE ("EMIT", 4, 0, _EMIT, _KEY) {
    REG(a);

    PPOP(a);
    fputc(a, stdout);
}


// ( -- addr len )
PRIMITIVE ("WORD", 4, 0, _WORD, _EMIT) {
    static char buf[32];
    REG(a);
    REG(b);

    /* First skip leading whitespace or skip-to-eol's */
    b = 0; // Flag for skip-to-eol
    do {
        _KEY(NULL);
        PPOP(a);
        if (a == '\\')  b = 1;
        else if (b && a == '\n')  b = 0;
    } while ( b || a == ' ' || a == '\t' || a == '\n' );

    /* Then start storing chars in the buffer */
    b = 0; // Position within array
    do {
        _KEY(NULL);
        PPOP(a);
        buf[b++] = a;
    } while ( b < MAX_WORD_LEN && a != ' ' && a != '\t' && a != '\n' );

    if (b < MAX_WORD_LEN) {
        // If the word we read was of a suitable length, return addr and len
        PPUSH((cell)(&buf));
        PPUSH(b);
    }
    else {
        // If we got to the end without finding whitespace, return some sort of error
        // FIXME really?
        PPUSH(0);
    }

}

// ( addr len -- value remainder )
PRIMITIVE ("NUMBER", 6, 0, _NUMBER, _WORD) {
    char buf[MAX_WORD_LEN + 1];
    char *endptr;
    REG(a);
    REG(b);

    memset(buf, 0, MAX_WORD_LEN + 1);

    PPOP(a);  // len
    PPOP(b);  // addr

    memcpy(buf, (const char *) b, a);

    a = strtol(buf, &endptr, 0);        // value parsed
    b = strlen(buf) - (endptr - buf);   // number of chars left unparsed

    PPUSH(a);
    PPUSH(b);
}

// ( addr len -- addr )
PRIMITIVE ("FIND", 4, 0, _FIND, _NUMBER) {
    char *word;
    size_t len;
    REG(a);

    PPOP(len);
    PPOP(a);
    word = (char *) a;

    DictEntry *de = (DictEntry *) *var_LATEST;
    DictEntry *result = NULL;

    while (de != &_dict_DICT_ROOT) {
        if (len == (de->name_length & (F_HIDDEN | F_LENMASK))) { // tricky - ignores hidden words
            if (strncmp(word, de->name, len) == 0) {
                // found a match
                result = de;
            }
        }
        de = de->link;
    }

    PPUSH((cell) result);
}

// ( addr -- cfa )
PRIMITIVE (">CFA", 4, 0, _TCFA, _FIND) {
/*
struct _dict_entry  *link;
uint8_t             name_length; 
char                name[MAX_WORD_LEN];
pvf                 code;
*/
    REG(a);

    PPOP(a);
    PPUSH(a + sizeof(DictEntry*) + sizeof(uint8_t) + MAX_WORD_LEN);
}

// ( addr -- dfa )
PRIMITIVE (">DFA", 4, 0, _TDFA, _TCFA) {
    REG(a);

    PPOP(a);
    PPUSH(a + sizeof(DictEntry*) + sizeof(uint8_t) + MAX_WORD_LEN + sizeof(pvf));
}

// ( FIXME )
PRIMITIVE ("CREATE", 6, 0, _CREATE, _TDFA) {
    // FIXME
}

// FIXME other stuff




/***************************************************************************
    The LATEST variable denotes the top of the dictionary.  Its initial
    value points to its own dictionary entry (tricky).
    IMPORTANT:
    * Be sure to update its link pointer if you add more builtins before it!
    * This must be the LAST entry added to the dictionary!
 ***************************************************************************/
VARIABLE (LATEST, 6, (cell) &_dict_var_LATEST, _CREATE);  // FIXME keep this updated!

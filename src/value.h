// value.h
#ifndef VALUE_H
#define VALUE_H

typedef struct ASTNode ASTNode;  // forward-declare to avoid circular include

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_NULL,
    VAL_FUNC
} ValueType;

// Functions are stored as a descriptor in the environment like any other value
typedef struct {
    char     name[64];
    char     params[8][64];
    int      param_count;
    ASTNode *body;         // points into the AST — not copied, not owned
} FuncDef;

typedef struct {
    ValueType type;
    union {
        double   num;      // VAL_NUMBER
        char    *str;      // VAL_STRING  — heap-allocated copy
        int      boolean;  // VAL_BOOL    — 1 or 0
        FuncDef *func;     // VAL_FUNC    — heap-allocated descriptor
    } as;
} Value;

// Constructors
Value make_number(double n);
Value make_string(const char *s);
Value make_bool(int b);
Value make_null(void);
Value make_func(const char *name, char params[][64], int nparams, ASTNode *body);

// Utilities
int  val_is_truthy(Value v);
void val_print(Value v);
void val_to_cstr(Value v, char *buf, int bufsize);

#endif
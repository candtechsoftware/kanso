#pragma once
#include "../base/base_inc.h"

typedef enum Regex_Node_Kind
{
    Regex_NODE_LITERAL,
    Regex_NODE_CONCAT,
    Regex_NODE_ALTERNATE,
    Regex_NODE_STAR,
    Regex_NODE_PLUS,
    Regex_NODE_QMARK,
    Regex_NODE_CHAR_CLASS,
} Regex_Node_Kind;

typedef struct Regex_Node_Child Regex_Node_Child;
struct Regex_Node_Child
{
    s64 left;
    s64 right;
};

typedef struct Regex_Node Regex_Node;
struct Regex_Node
{
    union
    {
        char             literal;
        Regex_Node_Child children;
        struct
        {
            s64 child;
        } repeat;
        struct
        {
            b32 bitmap[256];
        } char_class;
    };
};

internal inline Regex_Node regex_make_literal(Arena *arena, char c);
internal inline Regex_Node regex_make_concat(Arena *arena, char c);
internal inline Regex_Node regex_make_alternate(Arena *arena, char c);
internal inline Regex_Node regex_make_star(Arena *arena, char c);
internal inline Regex_Node regex_make_plus(Arena *arena, char c);
internal inline Regex_Node regex_make_qmark(Arena *arena, char c);
internal inline Regex_Node regex_make_char_class(Arena *arena, char c);

internal Regex_Node regex_parse(Arena *arena, String pattern); 


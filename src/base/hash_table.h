#pragma once
#include "arena.h"
#include "types.h"
#include "string_core.h"
#include "util.h"

#define XXH_STATIC_LINKING_ONLY
#include "../../third_party/xxhash/xxhash.h"

typedef struct Key_Value_Pair Key_Value_Pair;
struct Key_Value_Pair
{
    union
    {
        String key_string;
        void  *key_raw;
        u32    key_u32;
        u64    key_u64;
    };
    union
    {
        String value_string;
        void  *value_raw;
        u32    value_u32;
        u64    value_u64;
    };
};

typedef struct Bucket_Node Bucket_Node;
struct Bucket_Node
{
    Bucket_Node   *next;
    Key_Value_Pair v;
};

typedef struct Bucket_List Bucket_List;
struct Bucket_List
{
    Bucket_Node *first;
    Bucket_Node *last;
};

typedef struct Hash_Table Hash_Table;
struct Hash_Table
{
    u64          count;
    u64          cap;
    Bucket_List *buckets;
    Bucket_List  free_buckets;
};

internal void
bucket_list_concat_in_place(Bucket_List *list, Bucket_List *to_concat)
{
    if (to_concat->first)
    {
        if (list->first)
        {
            list->last->next = to_concat->first;
            list->last = to_concat->last;
        }
        else
        {
            list->first = to_concat->first;
            list->last = to_concat->last;
        }
        MemoryZeroStruct(to_concat);
    }
}

internal Bucket_Node *
bucket_list_pop(Bucket_List *list)
{
    Bucket_Node *res = list->first;
    SLLQueuePop(list->first, list->last);
    return res;
}

internal u64
hash_table_hash(String string)
{
    XXH64_hash_t hash = XXH3_64bits(string.data, string.size);
    return hash;
}

internal Hash_Table *
hash_table_create(Arena *arena, u64 cap)
{
    Hash_Table *table = push_array(arena, Hash_Table, 1);
    table->cap = cap;
    table->buckets = push_array(arena, Bucket_List, cap);
    return table;
}

internal u64
hash_key_u64(u64 key)
{
    return XXH3_64bits(&key, sizeof(u64));
}

internal u64
hash_key_u32(u32 key)
{
    return XXH3_64bits(&key, sizeof(u32));
}

internal u64
hash_key_string(String key)
{
    return XXH3_64bits(key.data, key.size);
}

internal Bucket_Node *
hash_table_push_u64_u64(Arena *arena, Hash_Table *table, u64 key, u64 value)
{
    u64 hash = hash_key_u64(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    // Check if key already exists
    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (node->v.key_u64 == key)
        {
            node->v.value_u64 = value;
            return node;
        }
    }

    // Add new node
    Bucket_Node *new_node = push_array(arena, Bucket_Node, 1);
    new_node->v.key_u64 = key;
    new_node->v.value_u64 = value;
    new_node->next = NULL;

    if (!bucket->first)
    {
        bucket->first = bucket->last = new_node;
    }
    else
    {
        bucket->last->next = new_node;
        bucket->last = new_node;
    }

    table->count++;
    return new_node;
}

internal Bucket_Node *
hash_table_push_u32_u32(Arena *arena, Hash_Table *table, u32 key, u32 value)
{
    u64 hash = hash_key_u32(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    // Check if key already exists
    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (node->v.key_u32 == key)
        {
            node->v.value_u32 = value;
            return node;
        }
    }

    // Add new node
    Bucket_Node *new_node = push_array(arena, Bucket_Node, 1);
    new_node->v.key_u32 = key;
    new_node->v.value_u32 = value;
    new_node->next = NULL;

    if (!bucket->first)
    {
        bucket->first = bucket->last = new_node;
    }
    else
    {
        bucket->last->next = new_node;
        bucket->last = new_node;
    }

    table->count++;
    return new_node;
}

internal Bucket_Node *
hash_table_push_string_string(Arena *arena, Hash_Table *table, String key, String value)
{
    u64 hash = hash_key_string(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    // Check if key already exists
    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (str_match(node->v.key_string, key))
        {
            node->v.value_string = value;
            return node;
        }
    }

    // Add new node
    Bucket_Node *new_node = push_array(arena, Bucket_Node, 1);
    new_node->v.key_string = key;
    new_node->v.value_string = value;
    new_node->next = NULL;

    if (!bucket->first)
    {
        bucket->first = bucket->last = new_node;
    }
    else
    {
        bucket->last->next = new_node;
        bucket->last = new_node;
    }

    table->count++;
    return new_node;
}

internal Key_Value_Pair *
hash_table_search_u64(Hash_Table *table, u64 key)
{
    u64 hash = hash_key_u64(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (node->v.key_u64 == key)
        {
            return &node->v;
        }
    }

    return NULL;
}

internal Key_Value_Pair *
hash_table_search_u32(Hash_Table *table, u32 key)
{
    u64 hash = hash_key_u32(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (node->v.key_u32 == key)
        {
            return &node->v;
        }
    }

    return NULL;
}

internal Key_Value_Pair *
hash_table_search_string(Hash_Table *table, String key)
{
    u64 hash = hash_key_string(key);
    u64 idx = hash % table->cap;

    Bucket_List *bucket = &table->buckets[idx];

    for (Bucket_Node *node = bucket->first; node; node = node->next)
    {
        if (str_match(node->v.key_string, key))
        {
            return &node->v;
        }
    }

    return NULL;
}

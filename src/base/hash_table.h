#pragma once
#include "arena.h"
#include "types.h"
#include "string_core.h"
#include "util.h"
#include "arena.h"

#define XXH_STATIC_LINKING_ONLY
#include "../../third_party/xxhash/xxhash.h"

template<typename K, typename V>
struct Key_Value_Pair
{
    K key;
    V value;
};

template<typename K, typename V>
struct Bucket_Node
{
    Bucket_Node<K, V>*   next;
    Key_Value_Pair<K, V> v;
};

template<typename K, typename V>
struct Bucket_List
{
    Bucket_Node<K, V>* first = nullptr;
    Bucket_Node<K, V>* last  = nullptr;
};

template<typename K, typename V>
struct Hash_Table
{
    u64 count;
    u64 cap;
    Bucket_List<K, V>* buckets = nullptr;
    Bucket_List<K, V>* free_buckets;
};

template<typename K, typename V>
internal void
bucket_list_concat_in_place(Bucket_List<K, V>* list, Bucket_List<K, V>* to_concat)
{
    if (to_concat->first) {
        if (list->first) {
            list->last->next = to_concat->first;
            list->last       = to_concat->last;
        } else {
            list->first = to_concat->first;
            list->last  = to_concat->last;
        }
        MemoryZeroStruct(to_concat);
    }
}

template<typename K, typename V>
internal Bucket_Node<K, V>*
bucket_list_pop(Bucket_List<K, V>* list)
{
    Bucket_Node<K, V>* res = list->first;
    SLLQueuePop(list->first, list->last);
}

internal u64
hash_table_hash(String string)
{
    XXH64_hash_t hash = XXH3_64bits(string.data, string.size);
    return hash;
}

template<typename K, typename V>
internal Hash_Table<K, V>*
hash_table_create(Arena* arena, u64 cap)
{
    typedef Hash_Table<K, V>  HashTableType;
    typedef Bucket_List<K, V> BucketListType;

    Hash_Table<K, V>* table = push_array(arena, HashTableType, 1);
    table->cap     = cap;
    table->buckets = push_array(arena, BucketListType, cap);
    return table;
}

template<typename K>
internal u64
hash_key(K key)
{
    return XXH3_64bits(&key, sizeof(K));
}

template<>
u64
hash_key<u64>(u64 key)
{
    return XXH3_64bits(&key, sizeof(u64));
}

template<>
u64
hash_key<u32>(u32 key)
{
    return XXH3_64bits(&key, sizeof(u32));
}

template<typename K, typename V>
internal void
hash_table_insert(Hash_Table<K, V>* table, K key, V value, Arena* arena)
{
    u64 hash = hash_key(key);
    u64 idx = hash % table->cap;
    
    Bucket_List<K, V>* bucket = &table->buckets[idx];
    
    // Check if key already exists
    for (Bucket_Node<K, V>* node = bucket->first; node; node = node->next) {
        if (node->v.key == key) {
            node->v.value = value;
            return;
        }
    }
    
    // Add new node
    using NodeType = Bucket_Node<K, V>;
    Bucket_Node<K, V>* new_node = push_array(arena, NodeType, 1);
    new_node->v.key = key;
    new_node->v.value = value;
    new_node->next = nullptr;
    
    if (!bucket->first) {
        bucket->first = bucket->last = new_node;
    } else {
        bucket->last->next = new_node;
        bucket->last = new_node;
    }
    
    table->count++;
}

template<typename K, typename V>
internal V*
hash_table_find(Hash_Table<K, V>* table, K key)
{
    u64 hash = hash_key(key);
    u64 idx = hash % table->cap;
    
    Bucket_List<K, V>* bucket = &table->buckets[idx];
    
    for (Bucket_Node<K, V>* node = bucket->first; node; node = node->next) {
        if (node->v.key == key) {
            return &node->v.value;
        }
    }
    
    return nullptr;
}

template<typename K, typename V>
internal void
hash_table_remove(Hash_Table<K, V>* table, K key)
{
    u64 hash = hash_key(key);
    u64 idx = hash % table->cap;
    
    Bucket_List<K, V>* bucket = &table->buckets[idx];
    
    Bucket_Node<K, V>* prev = nullptr;
    for (Bucket_Node<K, V>* node = bucket->first; node; prev = node, node = node->next) {
        if (node->v.key == key) {
            if (prev) {
                prev->next = node->next;
            } else {
                bucket->first = node->next;
            }
            
            if (node == bucket->last) {
                bucket->last = prev;
            }
            
            // Note: We don't free memory since we're using arenas
            table->count--;
            return;
        }
    }
}

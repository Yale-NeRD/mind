#ifndef __LIST_AND_HASH_H__
#define __LIST_AND_HASH_H__

/*
 * Based on hash table from https://github.com/bennettbuchanan/holbertonschool-low_level_programming/blob/master/hash_tables/hashtable.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include <stddef.h>

typedef struct hlist
{
    u64 key;
    void *value;
    struct hlist *next;
} hlist;

/*
 * @size : The size of the array
 *
 * @array : An array of size @size
 * Each cell of this array is a pointer to the first node of a linked list,
 * because we want our HashTable to use a Chaining collision handling
 */
typedef struct hash_table
{
    unsigned int size;
    hlist **array;
} hash_table;

/*
 * @key : The key to hash
 *
 * @size : The size of the hashtable
 *
 * @return : An integer N like 0 <= N < @size
 * This integer represents the index of @key in an array of size @size
 */
u8 hash_ftn_u8(u16 tgid);
u16 hash_ftn_u16(u64 key);
hash_table *ht_create(hash_table *ht, unsigned int size);
hlist *ht_get_list_head(hash_table *hashtable, unsigned int hash);
void *ht_get(hash_table *hashtable, unsigned int hash, u64 key);
hlist *ht_get_node(hash_table *hashtable, unsigned int hash, u64 key);
int ht_put(hash_table *hashtable, unsigned int hash, u64 key, void *value); // key, value
void ht_free(hash_table *);
void ht_free_node(hash_table *, unsigned int hash, hlist *); // actual value (pointer) is not freed

/* Doubly linked list */
struct list_node
{
    void *data;
    struct list_node *next;
    struct list_node *prev;
};

struct unique_tgid_node;
void list_init(struct list_node *head);
void list_insert_after(struct list_node *node, void *data_ptr);
void list_insert_at_head(struct list_node *head, void *data_ptr);
void list_insert_at_tail(struct list_node *head, void *data_ptr);
void list_delete_utgid(struct list_node *head, struct unique_tgid_node *node);
void list_delete_node(struct list_node *head, struct list_node *node);
void list_delete_node_no_header(struct list_node *node);
void list_detach_node(struct list_node *node);

#endif

/* Please check list_and_hash.h for the referenced repo */
#include "controller.h"
#include "list_and_hash.h"
#include "request_handler.h"

u8 hash_ftn_u8(u16 tgid)
{
    u8 hash = tgid % (1 << MN_PID_HASH_BIT);
    return hash;
}

u16 hash_ftn_u16(u64 key)
{
    u16 hash = key % (1 << MN_CACHE_HASH_BIT);
    return hash;
}

hash_table *ht_create(hash_table *ht, unsigned int size)
{
    if (!ht)
    {
        return NULL;
    }

    if (size < 1)
    {
        return NULL;
    }

    ht->array = (hlist **)malloc(size * sizeof(hlist *));
    if (ht->array == NULL)
    {
        return (NULL);
    }

    memset(ht->array, 0, size * sizeof(hlist *));

    ht->size = size;

    return ht;
}

void ht_free(hash_table *hashtable) // This function also frees value
{
    hlist *tmp;
    unsigned long i;  // Prevent overflow (instead of using u8)
    unsigned long size;

    if (hashtable == NULL)
    {
        return;
    }

    // printf("Clean hash table - size: %lu\n", hashtable->size);
    size = hashtable->size;
    for (i = 0; i < size; ++i)
    {
        while (hashtable->array[i])
        {
            
            tmp = hashtable->array[i]->next;
            if (hashtable->array[i]->value)
            {
                free(hashtable->array[i]->value);
            }
            free(hashtable->array[i]);
            hashtable->array[i] = tmp;
        }
    }
    free(hashtable->array);
    hashtable->array = NULL;
}

void _ht_free_node(hash_table *hashtable, unsigned int hash, hlist *hnode, int remove_node)
{
    hlist *tmp, *prv;

    if (hashtable == NULL || hashtable->size <= hash)
    {
        return;
    }

    if (hashtable->array[hash] != NULL)
    {
        // We assume there is no duplicated node
        // We erase node based on ptr not key
        prv = tmp = hashtable->array[hash];
        while (tmp && (tmp != hnode))
        {
            prv = tmp;
            tmp = tmp->next;
        }

        if (!tmp)
        {
            ; // not found
        }
        else if (tmp == prv)
        {
            // Fonund, first node
            hashtable->array[hash] = tmp->next; // Can be NULL
            if (remove_node)
                free(tmp);
        }else{
            // Found
            prv->next = tmp->next;  // Can be NULL
            if (remove_node)
                free(tmp);
        }
    }
}

void ht_free_node(hash_table *hashtable, unsigned int hash, hlist *hnode)
{
    _ht_free_node(hashtable, hash, hnode, 1);
}

hlist *ht_get_list_head(hash_table *hashtable, unsigned int hash)
{
    if (hashtable == NULL || hashtable->size <= hash)
    {
        return NULL;
    }
    return hashtable->array[hash];
}

void *_ht_get(hash_table *hashtable, unsigned int hash, u64 key, int return_value)
{
    hlist *tmp;
    if (hashtable == NULL || hashtable->size <= hash)
    {
        return NULL;
    }
    tmp = hashtable->array[hash];

    while (tmp != NULL)
    {
        if (tmp->key == key)
        {
            break;
        }
        tmp = tmp->next;
    }

    if (tmp == NULL)
    {
        return NULL;
    }
    if (return_value)
        return tmp->value;
    else
        return tmp;
}

void *ht_get(hash_table *hashtable, unsigned int hash, u64 key)
{
    return _ht_get(hashtable, hash, key, 1);
}

hlist *ht_get_node(hash_table *hashtable, unsigned int hash, u64 key)
{
    return (hlist *)_ht_get(hashtable, hash, key, 0);
}

// Range of hash was already checked
static void node_put_new_node(hash_table *hashtable, unsigned int hash, hlist *node)
{
    if (hashtable->array[hash] != NULL)
    {
        hlist *tmp = hashtable->array[hash];
        while (tmp)
        {
            if (tmp->key == node->key)
            {
                break;
            }
            tmp = tmp->next;
        }

        // If not found
        if (tmp == NULL)
        {
            node->next = hashtable->array[hash];
            hashtable->array[hash] = node;
        }
        else
        {
            fprintf(stderr, "Existing node with the same key (%lu)\n", node->key);
        }
    }
    else
    {
        node->next = NULL;
        hashtable->array[hash] = node;
    }
}

int ht_put(hash_table *hashtable, unsigned int hash, u64 key, void *value)
{
    hlist *node;

    if (hashtable == NULL || hashtable->size <= hash)
    {
        pr_others("WARNING - hash table: 0x%lx, size: %u, hash: %u, key: 0x%lx\n",
                  (unsigned long)hashtable, (hashtable == NULL) ? 0 : hashtable->size, hash, key);
        return 1;
    }

    node = malloc(sizeof(hlist));
    if (node == NULL)
    {
        return (1);
    }
    memset(node, 0, sizeof(hlist));

    node->key = key;
    node->value = value;

    node_put_new_node(hashtable, hash, node);

    return 0;
}

/* === List related == */
// (tail) <-> head (empty) <-> first node <-> ... <-> tail <-> (head)
void list_init(struct list_node *head)
{
    head->next = head;
    head->prev = head;
    head->data = NULL;
}

// Creates a new Node and returns pointer to it.
static struct list_node *list_create_new_node(void *data_ptr)
{
    struct list_node *newNode = (struct list_node *)malloc(sizeof(struct list_node));
    newNode->data = data_ptr;
    newNode->prev = NULL;
    newNode->next = NULL;
    return newNode;
}

// Inserts a Node at head of doubly linked list
void list_insert_after(struct list_node *node, void *data_ptr)
{

    if (node)
    {
        struct list_node *newNode = list_create_new_node(data_ptr);
        if (node->next)
        {
            node->next->prev = newNode;
            newNode->next = node->next;
        }
        node->next = newNode;
        newNode->prev = node;
    }
}

void list_insert_at_head(struct list_node *head, void *data_ptr)
{
    list_insert_after(head, (void*)data_ptr);
}

// Inserts a node at tail of doubly linked list
void list_insert_at_tail(struct list_node *head, void *data_ptr)
{
    struct list_node *newNode;
    if (!head)
    {
        return;
    }
    newNode = list_create_new_node(data_ptr);

    if (head->next == NULL || head->next == head)
    {
        newNode->prev = head;
        head->next = newNode;
        newNode->next = head;
        head->prev = newNode;
        return;
    }

    newNode->prev = head->prev;
    head->prev->next = newNode;
    newNode->next = head;
    head->prev = newNode;
}

void list_delete_by_data(struct list_node *head, void *data)
{
    struct list_node *tmp;
    if (!head || !head->next || head->next == head)  // Emtpy list
    {
        return;
    }

    // Find node
    tmp = head->next;
    while (tmp && tmp->next != head)
    {
        if (tmp->data == data)
        {
            list_delete_node(head, tmp);
            return;
        }
        tmp = tmp->next;    // Go to the next node
    }
}

void list_delete_utgid(struct list_node *head, struct unique_tgid_node *node)
{
    list_delete_by_data(head, (void*)node);
}

void list_delete_node(struct list_node *head, struct list_node *node)
{
    (void)head;
    list_delete_node_no_header(node);
}

void list_delete_node_no_header(struct list_node *node)
{
    if (node && node->next != node) // Not for the header
    {
        node->prev->next = node->next; // Can be NULL
        if (node->next)
        {
            node->next->prev = node->prev; // Can be NULL
        }
        free(node);
    }
}

void list_detach_node(struct list_node *node)
{
    if (node && node->next != node) // Not for the header
    {
        node->prev->next = node->next; // Can be NULL
        if (node->next)
        {
            node->next->prev = node->prev; // Can be NULL
        }
    }
}

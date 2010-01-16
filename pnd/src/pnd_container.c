
#include <stdlib.h> // for malloc, free, etc
#include <string.h> // for strdup()

#include "pnd_container.h"

// This is implemented in a very vaguely 'class-like' fashion; sure, I could use union's and
// void*'s to payload areas and so forth, but this method is pretty convenient instead. We allocate
// a buffer of the size of the node PLUS the user requested payload, and then return the payload
// part of it. We can go back to the node part with some basic pointer subtraction whenever we
// want, and are reasonably assured the user won't monkey with anything in the mechanism.
//  In short, we can change or replace the container code without impacting the user, and without
// boring anyone with void*s or unions or sub-structs and the like.

// a Box node is just a key-name and a pointer to the next; the payload is whatever data
// follows this. The container itself doesn't care.
struct pnd_box_node_t
{
    char *key;
    struct pnd_box_node_t *next;
};

struct pnd_box_t
{
    char *name;             // for when you're using gdb and wondering wtf you're looking at
    struct pnd_box_node_t *head;   // the first node
};

#define PAYLOAD2NODE(x)((struct pnd_box_node_t*)(((unsigned char *)(x))- sizeof(struct pnd_box_node_t)))
#define NODE2PAYLOAD(x)(((unsigned char *)(x))+ sizeof(struct pnd_box_node_t))

pnd_box_handle pnd_box_new(char *name)
{

    struct pnd_box_t *p = malloc(sizeof(struct pnd_box_t));

    if (! p)
    {
        return(NULL); // youch!
    }

    if (name)
    {
        p -> name = strdup(name);
    }
    else
    {
        p -> name = NULL;
    }

    p -> head = NULL;

    return(p);
}

void pnd_box_delete(pnd_box_handle box)
{
    struct pnd_box_t *p = (struct pnd_box_t*)box;
    struct pnd_box_node_t *n, *next;

    /* free up the list
     */

    n = p -> head;

    while (n)
    {

        if (n -> key)
        {
            free(n->key);
        }

        next = n->next;

        free(n);

        n = next;

    } // while

    /* free up the box itself
     */

    if (p -> name)
    {
        free(p -> name);
    }

    p -> head = (void*)123; // if you're looking at a stale pointer in gdb, this might tip you off

    free(p);

    return;
}

void *pnd_box_allocinsert(pnd_box_handle box, char *key, unsigned int size)
{
    struct pnd_box_t *p = (struct pnd_box_t*)box;

    struct pnd_box_node_t *n = malloc(sizeof(struct pnd_box_node_t) + size);

    if (! n)
    {
        return(NULL); // must be getting bloody tight!
    }

    memset(n, '\0', sizeof(struct pnd_box_node_t) + size);

    if (key)
    {
        n -> key = strdup(key);
    }
    else
    {
        n -> key = NULL;
    }

    n -> next = p -> head;

    p -> head = n;

    return(NODE2PAYLOAD(n));
}

void *pnd_box_find_by_key(pnd_box_handle box, char *key)
{
    struct pnd_box_t *p = (struct pnd_box_t*)box;
    struct pnd_box_node_t *n;

    n = p -> head;

    while (n)
    {

        if (strcmp(n -> key, key) == 0)
        {
            return(NODE2PAYLOAD(n));
        }

        n = n -> next;
    } // while

    return(NULL);
}

char *pnd_box_get_name(pnd_box_handle box)
{
    struct pnd_box_t *p = (struct pnd_box_t*)box;
    return(p -> name);
}

void *pnd_box_get_head(pnd_box_handle box)
{
    struct pnd_box_t *p = (struct pnd_box_t*)box;

    if (! p -> head)
    {
        return(NULL);
    }

    return(NODE2PAYLOAD(p -> head));
}

void *pnd_box_get_next(void *node)
{
    struct pnd_box_node_t *p = PAYLOAD2NODE(node);
    p = p -> next;

    if (! p)
    {
        return(NULL);
    }

    return(NODE2PAYLOAD(p));
}

char *pnd_box_get_key(void *node)
{
    struct pnd_box_node_t *p = PAYLOAD2NODE(node);
    return(p -> key);
}

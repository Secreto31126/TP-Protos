#include <closed_hashing.h>

#include <stdlib.h>
#include <stdio.h>

#define RESIZE_FACTOR 4
#define THRESHOLD 0.5
#define DUMMY ((void *)1)

char hashset_insert(hashset *set, void *element);
void check_resize_hashset(hashset *set);
void resize_hashset(hashset *set);

hashset *new_hashset(hash_function hasher, compare_function compare, free_function free_f, uint64_t initial_dim)
{
    hashset *new_set = malloc(sizeof(hashset));
    new_set->hasher = hasher;
    new_set->compare = compare;
    new_set->free_f = free_f;
    new_set->elements = malloc(sizeof(void *) * initial_dim);
    for (uint64_t i = 0; i < initial_dim; i++)
    {
        new_set->elements[i] = NULL;
    }
    new_set->elements_dim = initial_dim;
    new_set->elements_size = 0;
    return new_set;
}

void resize_hashset(hashset *set)
{
    void **old_elements = set->elements;
    uint64_t old_elements_dim = set->elements_dim;
    set->elements_dim = set->elements_dim * RESIZE_FACTOR + 1;
    set->elements = malloc(sizeof(void *) * set->elements_dim);

    for (uint64_t i = 0; i < set->elements_dim; i++)
    {
        set->elements[i] = NULL;
    }
    for (uint64_t i = 0; i < old_elements_dim; i++)
    {
        if (old_elements[i] != NULL && old_elements[i] != DUMMY)
            hashset_insert(set, old_elements[i]);
    }
    free(old_elements);
}

void check_resize_hashset(hashset *set)
{
    if (((float)set->elements_size / set->elements_dim) > THRESHOLD)
        resize_hashset(set);
}

char hashset_insert(hashset *set, void *element)
{
    uint64_t hash_index = set->hasher(element) % set->elements_dim;
    void **elements = set->elements;
    while (elements[hash_index] != NULL && !set->compare(elements[hash_index], element) && elements[hash_index] != DUMMY)
    {
        hash_index++;
        hash_index %= set->elements_dim;
    }

    char to_return = 1;
    if (elements[hash_index] == NULL || elements[hash_index] == DUMMY)
        to_return = 0;
    else
        set->free_f(elements[hash_index]);

    elements[hash_index] = element;

    return to_return;
}

char hashset_add(hashset *set, void *element)
{
    check_resize_hashset(set);

    char overwrote = hashset_insert(set, element);

    if (!overwrote)
        set->elements_size++;
    return overwrote;
}

char hashset_delete(hashset *set, const void *element)
{
    uint64_t hash_index = set->hasher(element) % set->elements_dim;
    void **elements = set->elements;
    while (elements[hash_index] != NULL)
    {
        if (set->compare(elements[hash_index], element))
        {
            elements[hash_index] = DUMMY;
            set->elements_size--;
            return 1;
        }
        hash_index++;
        hash_index %= set->elements_dim;
    }
    return 0;
}

void *hashset_get(const hashset *set, const void *element)
{
    uint64_t hash_index = set->hasher(element) % set->elements_dim;
    void **elements = set->elements;

    while (elements[hash_index] != NULL)
    {

        uint64_t counter = 0;

        if (counter++ > set->elements_dim)
            break;

        if (set->compare(elements[hash_index], element))
            return elements[hash_index];

        hash_index++;
        hash_index %= set->elements_dim;
    }

    return NULL;
}

char hashset_contains(const hashset *set, const void *element)
{
    return hashset_get(set, element) != NULL;
}

uint64_t hashset_size(hashset *set)
{
    return set->elements_size;
}

void free_hashset(hashset *set)
{
    for (uint64_t i = 0; i < set->elements_dim; i++)
    {
        if (set->elements[i] != NULL && set->elements[i] != DUMMY)
            set->free_f(set->elements[i]);
    }
    free(set->elements);
    free(set);
}

hashset_iterator *hashset_get_iterator(const hashset *set)
{
    hashset_iterator *iterator = malloc(sizeof(hashset_iterator));
    iterator->set = set;
    iterator->index = 0;
    return iterator;
}

void *hashset_next(hashset_iterator *iterator)
{
    while (iterator->index < iterator->set->elements_dim && (iterator->set->elements[iterator->index] == DUMMY || iterator->set->elements[iterator->index] == NULL))
        iterator->index++;
    if (iterator->index < iterator->set->elements_dim)
        return iterator->set->elements[iterator->index++];
    return NULL;
}

char hashset_has_next(hashset_iterator *iterator)
{
    while (iterator->index < iterator->set->elements_dim && (iterator->set->elements[iterator->index] == DUMMY || iterator->set->elements[iterator->index] == NULL))
        iterator->index++;
    return iterator->index < iterator->set->elements_dim;
}

void free_hashset_iterator(hashset_iterator *iterator)
{
    free(iterator);
}

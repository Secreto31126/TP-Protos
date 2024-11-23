#ifndef CLSD_H
#define CLSD_H

#include <stdint.h>

typedef uint64_t (*hash_function)(const void *);
typedef char (*compare_function)(const void *, const void *);
typedef void (*free_function)(void *);

typedef struct hashset
{
    hash_function hasher;
    compare_function compare;
    free_function free_f;
    void **elements;
    uint64_t elements_dim;
    uint64_t elements_size;
} hashset;

typedef struct hashset_iterator
{
    const hashset *set;
    uint64_t index;
} hashset_iterator;

/**
 * @brief Creates a hashset for (void *) elements
 *
 * @param hasher Hashing function
 * @param compare Comparison function for (void *) elements
 * @param free_f Free function for (void *) elements
 * @param initial_dim Initial/expected size of the hashset
 * @return hashset*
 */
hashset *new_hashset(hash_function hasher, compare_function compare, free_function free_f, uint64_t initial_dim);

/**
 * @brief Add an element to the set
 *
 * @param set
 * @param element
 * @return char 1 if an element was overwritten, 0 otherwise
 */
char hashset_add(hashset *set, void *element);

/**
 * @brief Remove an element from the set
 *
 * @note The element does not need to be populated, it is only necessary to populate the values used to identify it in the compare_function
 * @param set
 * @param element
 * @return char 1 if an element was removed, 0 otherwise
 */
char hashset_delete(hashset *set, const void *element);

/**
 * @brief Whether an element is contained in the set
 *
 * @param set
 * @param element
 * @return char
 */
char hashset_contains(const hashset *set, const void *element);

/**
 * @brief Get an element from the set
 *
 * @param set
 * @param element
 * @return void*
 */
void *hashset_get(const hashset *set, const void *element);

hashset_iterator *hashset_get_iterator(const hashset *set);

void *hashset_next(hashset_iterator *iterator);

char hashset_has_next(hashset_iterator *iterator);

void free_hashset_iterator(hashset_iterator *iterator);

/**
 * @brief Free the hashset and execute the free_function on all elements within
 *
 * @param set
 */
void free_hashset(hashset *set);

uint64_t hashset_size(hashset *set);

#endif

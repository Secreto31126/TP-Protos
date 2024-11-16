#include "statistics.h"
#include <stdlib.h>
#include <string.h>

#define SMALL_PRIME 31
#define LOG(x) ((log *)(x))
#define U_LOG(x) ((user_logs *)(x))
#define BIG_PRIME 1000000007
#define BLOCK 32

typedef struct user_logs
{
    log **logs;
    char *username;
    uint64_t logs_size;
    uint64_t logs_dim;
} user_logs;

user_logs *new_user_logs(char *username)
{
    user_logs *logs = malloc(sizeof(user_logs));
    logs->logs = malloc(sizeof(log *) * BLOCK);
    logs->logs_dim = BLOCK;
    logs->logs_size = 0;
    logs->username = username;
    return logs;
}

void free_user_logs(user_logs *logs)
{
    free(logs->logs);
    free(logs);
}

void resize_user_logs(user_logs *logs)
{
    logs->logs_dim *= 2;
    logs->logs = realloc(logs->logs, logs->logs_dim * sizeof(logs));
}

void check_resize_user_logs(user_logs *logs)
{
    if (logs->logs_size == logs->logs_dim)
        resize_user_logs(logs);
}

void add_log(user_logs *logs, log *l)
{
    check_resize_user_logs(logs);
    logs->logs[logs->logs_size++] = l;
}

uint64_t hash_user_logs(const void *element)
{
    uint64_t sum = 0;
    for (char *s = U_LOG(element)->username; *s; s++)
        sum = sum * SMALL_PRIME + *s;
    return sum % BIG_PRIME;
}

char are_equal_logs(const void *l1, const void *l2)
{
    return !strcmp(U_LOG(l1)->username, U_LOG(l2)->username);
}

void deep_free_logs(void *logs)
{
    user_logs *l = U_LOG(logs);
    for (uint64_t i = 0; i < l->logs_size; i++)
    {
        free(l->logs[i]);
    }
    free_user_logs(l);
}

statistics_manager *create_statistics_manager()
{
    statistics_manager *sm = malloc(sizeof(statistics_manager));
    sm->current_connections = 0;
    sm->historic_connections = 0;
    sm->transferred_bytes = 0;
    sm->user_logs = new_hashset(hash_user_logs, are_equal_logs, deep_free_logs, BLOCK);
    return sm;
}

log *new_log(char *username, timestamp time, void *data, log_t type)
{
    log *l = malloc(sizeof(log));
    l->data = data;
    l->time = time;
    l->data = data;
    l->type = type;
    return l;
}

void destroy_statistics_manager(statistics_manager *sm)
{
    free_hashset(sm->user_logs);
    free(sm);
}

void add_log_to_hashset(hashset *set, log *l)
{
    user_logs dummy;
    dummy.username = l->username;
    user_logs *u_log = hashset_get(set, &dummy);
    if (u_log == NULL)
    {
        u_log = new_user_logs(l->username);
        hashset_add(set, l);
    }
    add_log(u_log, l);
}

void log_bytes_transferred(statistics_manager *sm, char *username, uint64_t bytes, timestamp time)
{
    sm->transferred_bytes += bytes;
}
void log_connect(statistics_manager *sm, char *username, timestamp time)
{
    sm->historic_connections++;
    sm->current_connections++;
    add_log_to_hashset(sm->user_logs, new_log(username, time, NULL, CONNECTION));
}
void log_disconnect(statistics_manager *sm, char *username, timestamp time)
{
    sm->current_connections--;
    add_log_to_hashset(sm->user_logs, new_log(username, time, NULL, DISCONNECTION));
}

void log_other(statistics_manager *sm, char *username, timestamp time, void *data)
{
    add_log_to_hashset(sm->user_logs, new_log(username, time, data, OTHER));
}

uint64_t get_all_logs(statistics_manager *sm, log *log_buffer, uint64_t log_buffer_size)
{
    hashset_iterator *iterator = hashset_get_iterator(sm->user_logs);
    uint64_t index = 0;
    while (hashset_has_next(iterator) && index < log_buffer_size)
    {
        user_logs *u_log = U_LOG(hashset_next(iterator));
        for (uint64_t i = 0; i < u_log->logs_size && index < log_buffer_size; i++)
        {
            log_buffer[index++] = *(u_log->logs[i]);
        }
    }
    return index - 1;
}

uint64_t get_user_logs(statistics_manager *sm, char *username, log *log_buffer, uint64_t log_buffer_size)
{
    user_logs dummy;
    dummy.username = username;

    user_logs *u_log = U_LOG(hashset_get(sm->user_logs, &dummy));
    if (u_log == NULL)
        return 0;
    uint64_t i;
    for (i = 0; i < u_log->logs_size && i < log_buffer_size; i++)
    {
        log_buffer[i] = *(u_log->logs[i]);
    }
    return i - 1;
}

uint64_t read_bytes_transferred(statistics_manager *sm, uint64_t bytes)
{
    return sm->transferred_bytes;
}

uint64_t read_historic_connections(statistics_manager *sm)
{
    return sm->historic_connections;
}

uint64_t read_current_connections(statistics_manager *sm)
{
    return sm->current_connections;
}

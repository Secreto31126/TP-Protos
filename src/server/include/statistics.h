#ifndef STSTC_H
#define STSTC_H
#include <stdint.h>
#include <time.h>
#include "closed_hashing.h"

typedef enum month_t
{
    JANUARY = 0,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
} month_t;

typedef struct tm timestamp;

typedef enum log_t
{
    CONNECTION,
    DISCONNECTION,
    BYTES_TRANSFERRED,
    OTHER
} log_t;

typedef struct log
{
    char *username;
    char *ip;
    timestamp time;
    void *data;
    log_t type;
} log;

typedef struct user_logs
{
    log **logs;
    char *username;
    uint64_t logs_size;
    uint64_t logs_dim;
} user_logs;

typedef struct statistics_manager
{
    uint64_t current_connections;
    uint64_t max_current_connections;
    uint64_t historic_connections;
    uint64_t transferred_bytes;
    hashset *user_logs;
    log **logs_array;
    uint64_t logs_array_dim;
    uint64_t logs_array_size;
} statistics_manager;

statistics_manager *create_statistics_manager();
void destroy_statistics_manager(statistics_manager *sm);
timestamp log_now();
char *readable_time(timestamp t);

void log_bytes_transferred(statistics_manager *sm, char *username, char *ip, uint64_t bytes, timestamp time);
void log_connect(statistics_manager *sm, char *username, char *ip, timestamp time);
void log_disconnect(statistics_manager *sm, char *username, char *ip, timestamp time);
void log_other(statistics_manager *sm, char *username, char *ip, timestamp time, void *data);
uint64_t get_all_logs(statistics_manager *sm, log *log_buffer, uint64_t log_buffer_size);
uint64_t get_user_logs(statistics_manager *sm, char *username, log *log_buffer, uint64_t log_buffer_size);
uint64_t get_all_logs_count(statistics_manager *sm);
uint64_t get_user_logs_count(statistics_manager *sm, char *username);
uint64_t get_all_logs_range(statistics_manager *sm, log *log_buffer, uint64_t range_start, uint64_t range_end);
uint64_t get_user_logs_range(statistics_manager *sm, char *username, log *log_buffer, uint64_t range_start, uint64_t range_end);
uint64_t read_bytes_transferred(statistics_manager *sm, uint64_t bytes);
uint64_t read_historic_connections(statistics_manager *sm);
uint64_t read_current_connections(statistics_manager *sm);
uint64_t read_max_current_connections(statistics_manager *sm);

#endif

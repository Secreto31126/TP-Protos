#include <stdint.h>

typedef struct statistics_manager
{
    uint64_t current_connections;
    uint64_t historic_connections;
    uint64_t transferred_bytes;
} statistics_manager;

statistics_manager *create_statistics_manager();
void destroy_statistics_manager(statistics_manager *sm);

void log_bytes_transferred(statistics_manager *sm, uint64_t bytes);
void log_connect(statistics_manager *sm);
void log_disconnect(statistics_manager *sm);
uint64_t read_bytes_transferred(statistics_manager *sm, uint64_t bytes);
uint64_t read_historic_connections(statistics_manager *sm);
uint64_t read_current_connections(statistics_manager *sm);

void deserealize(statistics_manager *sm, int fd);
void serialize(statistics_manager *sm, int fd);
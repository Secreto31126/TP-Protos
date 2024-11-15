#include "statistics.h"
#include <stdlib.h>

statistics_manager *create_statistics_manager()
{
    statistics_manager *sm = malloc(sizeof(statistics_manager));
    sm->current_connections = 0;
    sm->historic_connections = 0;
    sm->transferred_bytes = 0;
    return sm;
}

void destroy_statistics_manager(statistics_manager *sm)
{
    free(sm);
}

void log_bytes_transferred(statistics_manager *sm, uint64_t bytes)
{
    sm->transferred_bytes += bytes;
}
void log_connect(statistics_manager *sm)
{
    sm->historic_connections++;
    sm->current_connections++;
}
void log_disconnect(statistics_manager *sm)
{
    sm->current_connections--;
}
uint64_t read_bytes_transferred(statistics_manager *sm, uint64_t bytes)
{
    return sm->transferred_bytes;
}
uint64_t read_historic_connections(statistics_manager *sm){
    return sm -> historic_connections;
    }
uint64_t read_current_connections(statistics_manager *sm)
{
    return sm->current_connections;
}

void deserealize(statistics_manager *sm, int fd)
{
}
void serialize(statistics_manager *sm, int fd)
{
}
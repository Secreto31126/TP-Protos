#include "log_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BLOCK 32

char *log_type_string(log_t type)
{
    switch (type)
    {
    case CONNECTION:
        return "CONNECTION";
    case DISCONNECTION:
        return "DISCONNECTION";
    default:
        return "OTHER";
    }
}

char *parse_log(log l, data_parser parser)
{
    char *type_s = log_type_string(l.type);
    char *time_s = readable_time(l.time);
    char *data_s;
    if (parser != NULL && l.data != NULL && (data_s = parser(l.data)) != NULL)
    {
        char *__restrict__ __format = "%s: %s; %s. INFO: %s.";
        uint64_t len = strlen(time_s) + strlen(type_s) + strlen(l.username) + sizeof(__format) - 4;
        char *s = calloc(len, sizeof(char));
        snprintf(s, len, __format, time_s, type_s, l.username, data_s);
        free(data_s);
        return s;
    }
    char *__restrict__ __format = "%s: %s; %s.";
    uint64_t len = strlen(time_s) + strlen(type_s) + strlen(l.username) + sizeof(__format) - 3;
    char *s = calloc(len, sizeof(char));
    snprintf(s, len, __format, time_s, type_s, l.username);
    return s;
}

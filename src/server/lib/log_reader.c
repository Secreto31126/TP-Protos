#include "log_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define BLOCK 32
#define MAX_FORMAT_SIZE 32

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

char *aux_parse_log(uint64_t len, const char *__restrict__ __format, ...)
{
    int result;
    va_list args;

    va_start(args, __format);

    char *s = calloc(len, sizeof(char));
    vsnprintf(s, len, __format, args);
    va_end(args);
    return s;
}

char *parse_log(pop_log l, data_parser parser)
{
    char *type_s = log_type_string(l.type);
    char *time_s = readable_time(l.time);
    char *data_s;
    char *s;
    if (parser != NULL && l.data != NULL && (data_s = parser(l.data)) != NULL)
    {
        uint64_t len = strlen(time_s) + strlen(type_s) + strlen(l.username) + strlen(l.ip) + strlen(data_s) + MAX_FORMAT_SIZE;
        char *s;
        if (l.ip == NULL || !strcmp(l.ip, l.username))
        {
            char *__restrict__ __format = "%s: %s; %s. INFO: %s.";
            s = aux_parse_log(len, __format, time_s, type_s, l.username, data_s);
        }
        else
        {
            char *__restrict__ __format = "%s: %s; %s - %s. INFO: %s.";
            s = aux_parse_log(len, __format, time_s, type_s, l.username, l.ip, data_s);
        }
        free(data_s);
    }
    {
        uint64_t len = strlen(time_s) + strlen(type_s) + strlen(l.username) + strlen(l.ip) + MAX_FORMAT_SIZE;
        if (l.ip == NULL || !strcmp(l.ip, l.username))
        {
            char *__restrict__ __format = "%s: %s; %s.";
            s = aux_parse_log(len, __format, time_s, type_s, l.username);
        }
        else
        {

            char *__restrict__ __format = "%s: %s; %s - %s.";
            s = aux_parse_log(len, __format, time_s, type_s, l.username, l.ip);
        }
    }
    return s;
}

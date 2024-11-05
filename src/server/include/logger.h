#ifndef LOGGER_H
#define LOGGER_H

#ifdef DEVELOPMENT
#include <stdio.h>
#define LOG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#endif

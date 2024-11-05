#ifndef LOGGER_H
#define LOGGER_H

#ifdef DEVELOPMENT
#include <stdio.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif

#endif

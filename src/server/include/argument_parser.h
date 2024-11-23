#ifndef ARG_H
#define ARG_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netutils.h>

/**
 * @brief Make modifications to default settings or print information based on arguments recieved in main
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param progname The name of the program
 */
void parse_arguments(int argc, const char *argv[], const char *progname);

#endif

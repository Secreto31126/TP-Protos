#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netutils.h>

/**
 * @brief Make modifications to default settings or print information based on arguments recieved in main
 */
void parse_arguments(int argc, const char* argv[], struct sockaddr_in *address_pop, struct sockaddr_in *address_conf, const char *progname);

#define DIR_PATH "DSPS VEMOS"
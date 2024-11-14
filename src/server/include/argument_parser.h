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
 * @param address_pop The address for the POP3 server
 * @param address_conf The address for the configuration server
 * @param dir_path The path to the directory where the mails are stored
 */
void parse_arguments(int argc, const char *argv[], const char *progname, struct sockaddr_in *address_pop, struct sockaddr_in *address_conf, const char **dir_path);

#endif

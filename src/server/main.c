#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <logger.h>
#include <netutils.h>
#include <pop.h>
#include <argument_parser.h>

#define DEFAULT_PORT_POP 8080
#define DEFAULT_PORT_CONF 8081

#define PROG_NAME "server"

/**
 * @brief Configure the signal handlers and close the standard input.
 */
static void setup();

static bool done = false;

int main(int argc, const char *argv[])
{
    struct sockaddr_in address_pop = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(DEFAULT_PORT_POP),
    };

    struct sockaddr_in address_conf = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(DEFAULT_PORT_CONF),
    };

    const char *dir_path = NULL;

    parse_arguments(argc, argv, PROG_NAME, &address_pop, &address_conf, &dir_path);

    setup();

    int server_fd = start_server(&address_pop);

    if (server_fd < 0)
    {
        return EXIT_FAILURE;
    }

    LOG("Server listening on %s:%d...\n", inet_ntoa(address_pop.sin_addr), ntohs(address_pop.sin_port));

    pop_init(dir_path);
    return server_loop(server_fd, &done, handle_pop_connect, handle_pop_message, handle_pop_close);
}

static void sigterm_handler(const int signal)
{
    LOG("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

static void sigint_handler(const int signal)
{
    sigterm_handler(signal);
}

static void setup()
{
    close(STDIN_FILENO);
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);
}

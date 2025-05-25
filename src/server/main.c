#include <argument_parser.h>
#include <logger.h>
#include <management_config.h>
#include <netutils.h>
#include <pop.h>
#include <pop_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>

#define PROG_NAME "server"

/**
 * @brief Configure the signal handlers and close the standard input.
 */
static void setup();

static bool done = false;

int main(int argc, const char *argv[])
{
    parse_arguments(argc, argv, PROG_NAME);

    setup();

    struct sockaddr_in6 address_pop = get_pop_adport();

    int pop_fd = start_server(&address_pop);
    if (pop_fd < 0)
    {
        return EXIT_FAILURE;
    }

    add_server(pop_fd, &address_pop);

    LOG("Server listening on port %d...\n", ntohs(address_pop.sin6_port));

    struct sockaddr_in6 address_manager = get_manager_adport();

    int manager_fd = start_server(&address_manager);
    if (manager_fd < 0)
    {
        return EXIT_FAILURE;
    }

    add_server(manager_fd, &address_manager);

    LOG("Manager listening on port %d...\n", ntohs(address_manager.sin6_port));

    statistics_manager *stats = create_statistics_manager();

    pop_init(NULL, manager_fd, stats);
    int r = server_loop(&done, handle_pop_connect, handle_pop_message, handle_pop_close, stats);
    pop_stop();

    destroy_statistics_manager(stats);

    return r;
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

static void sigchld_handler(const int signal)
{
    waitpid(-1, NULL, WNOHANG);
}

static void setup()
{
    close(STDIN_FILENO);
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGCHLD, sigchld_handler);
}

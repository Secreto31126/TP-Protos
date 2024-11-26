#include <argument_parser.h>
#include <logger.h>
#include <netutils.h>
#include <pop.h>
#include <pop_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>

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
    parse_arguments(argc, argv, PROG_NAME);

    setup();

    struct sockaddr_in address_pop = get_pop_adport();

    int pop_fd = start_server(&address_pop);
    if (pop_fd < 0)
    {
        return EXIT_FAILURE;
    }

    add_server(pop_fd, &address_pop);

    LOG("Server listening on %s:%d...\n", inet_ntoa(address_pop.sin_addr), ntohs(address_pop.sin_port));

    struct sockaddr_in address_manager = get_manager_adport();

    int manager_fd = start_server(&address_manager);
    if (manager_fd < 0)
    {
        return EXIT_FAILURE;
    }

    add_server(manager_fd, &address_manager);

    LOG("Manager listening on %s:%d...\n", inet_ntoa(address_manager.sin_addr), ntohs(address_manager.sin_port));

    pop_init(NULL);
    int r = server_loop(&done, handle_pop_connect, handle_pop_message, handle_pop_close);
    pop_stop();

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

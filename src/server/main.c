#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <logger.h>
#include <netutils.h>
#include <pop.h>

#define PORT 8080

/**
 * @brief Configure the signal handlers and close the standard input.
 */
static void setup();

static bool done = false;

int main()
{
    setup();

    struct sockaddr_in address;
    int server_fd = start_server(&address, PORT);

    if (server_fd < 0)
    {
        return EXIT_FAILURE;
    }

    LOG("Server listening on %s:%d...\n", inet_ntoa(address.sin_addr), PORT);

    pop_init(NULL);
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

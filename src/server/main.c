#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <logger.h>
#include <netutils.h>

#define PORT 8080

/**
 * @brief Configure the signal handlers and close the standard input.
 */
static void setup();
static int echo_back(const int client_fd, const char *body);

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

    return server_loop(server_fd, &done, echo_back);
}

static int echo_back(const int client_fd, const char *body)
{
    // Echo back
    if (send(client_fd, body, strlen(body), 0) < 0)
    {
        LOG("Failed to send message back to client\n");
        return CONNECTION_ERROR;
    }

    return KEEP_CONNECTION_OPEN;
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

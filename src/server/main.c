#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <logger.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define MAX_PENDING_CLIENTS 10

typedef enum ON_MESSAGE_RESULT
{
    KEEP_CONNECTION_OPEN = 0,
    CLOSE_CONNECTION = 1,
    CONNECTION_ERROR = -1
} ON_MESSAGE_RESULT;

/**
 * @brief Handle a message event
 *
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @return KEEP_CONNECTION_OPEN to keep the connection open.
 * @return CLOSE_CONNECTION to close.
 * @return CONNECTION_ERROR to save to stats and close.
 */
typedef ON_MESSAGE_RESULT (*message_event)(const int client_fd, const char *body);

/**
 * @brief Configure the signal handlers and close the standard input.
 */
static void setup();
/**
 * @brief Initialize a TCP server in non-blocking mode.
 *
 * @param address The server address binded.
 * @return int The server file descriptor, or -1 if an error occurred.
 */
static int start_server(struct sockaddr_in *address);
/**
 * @brief The main server loop to handle incoming connections and messages.
 *
 * @note The server will run until a SIGINT or SIGTERM signal is received, which will set the done flag to true.
 * @param server_fd The server file descriptor.
 * @param address The server address binded.
 * @param on_message The callback function to handle incoming messages.
 * @return int The exit status.
 */
static int server_loop(int server_fd, struct sockaddr_in address, message_event on_message);
static void sigterm_handler(const int signal);
static void sigint_handler(const int signal);

static bool done = false;

static int echo_back(const int client_fd, const char *body)
{
    // Echo back
    send(client_fd, body, strlen(body), 0);
    return CLOSE_CONNECTION;
}

int main()
{
    setup();

    struct sockaddr_in address;
    int server_fd = start_server(&address);

    if (server_fd < 0)
    {
        return EXIT_FAILURE;
    }

    LOG("Server listening on port %d...\n", PORT);

    return server_loop(server_fd, address, echo_back);
}

static int start_server(struct sockaddr_in *address)
{
    int server_fd;
    int opt = 1;

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("socket failed");
        return -1;
    }

    // Set the server socket settings
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        return -1;
    }

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)address, sizeof(*address)) < 0)
    {
        perror("bind failed");
        return -1;
    }

    // Set the server socket to non-blocking mode
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0)
    {
        perror("fcntl failed");
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_fd, MAX_PENDING_CLIENTS) < 0)
    {
        perror("listen failed");
        return -1;
    }

    return server_fd;
}

static int server_loop(int server_fd, struct sockaddr_in address, message_event on_message)
{
    int new_socket, addrlen = sizeof(address);

    // Array to hold client sockets and poll event types
    struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    while (!done)
    {
        int activity = poll(fds, nfds, -1);
        if (activity < 0)
        {
            perror("poll error");
            return EXIT_FAILURE;
        }

        // Check for new connections on the server socket
        if (fds[0].revents & POLLIN)
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                return EXIT_FAILURE;
            }

            LOG("New connection: socket fd %d\n", new_socket);

            // Add new socket to fds array
            fds[nfds].fd = new_socket;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        // Check each client socket for activity
        for (int i = 1; i < nfds; i++)
        {
            // Skip sockets without updates
            if (!(fds[i].revents & POLLIN))
            {
                continue;
            }

            char buffer[1024] = {0};
            int valread = read(fds[i].fd, buffer, sizeof(buffer));

            // Connection closed or error, remove from poll
            if (valread <= 0)
            {
                LOG("Client disconnected: socket fd %d\n", fds[i].fd);

                close(fds[i].fd);
                fds[i] = fds[--nfds];

                continue;
            }

            LOG("Received from client %d: %s\n", fds[i].fd, buffer);

            ON_MESSAGE_RESULT result = on_message(fds[i].fd, buffer);

            if (result != KEEP_CONNECTION_OPEN) {
                if (result == CONNECTION_ERROR) {
                    // TODO: Real stats
                    printf("Error handling message\n");
                }

                LOG("Closing connection: socket fd %d\n", fds[i].fd);

                close(fds[i].fd);
                fds[i] = fds[--nfds];
            }
        }
    }

    return EXIT_SUCCESS;
}

static void sigterm_handler(const int signal)
{
    printf("signal %d, cleaning up and exiting\n", signal);
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

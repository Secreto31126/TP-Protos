#ifndef NETUTILS_H_CTCyWGhkVt1pazNytqIRptmAi5U
#define NETUTILS_H_CTCyWGhkVt1pazNytqIRptmAi5U

#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>

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
 * @param length The message length.
 * @return KEEP_CONNECTION_OPEN to keep the connection open.
 * @return CLOSE_CONNECTION to close.
 * @return CONNECTION_ERROR to save to stats and close.
 */
typedef ON_MESSAGE_RESULT (*message_event)(const int client_fd, const char *body, size_t length);

/**
 * @brief Initialize a TCP server in non-blocking mode.
 *
 * @param address The server address binded.
 * @param port The server port.
 * @return int The server file descriptor, or -1 if an error occurred.
 */
int start_server(struct sockaddr_in *address, int port);
/**
 * @brief The main server loop to handle incoming connections and messages.
 *
 * @note The server will run until a SIGINT or SIGTERM signal is received, which will set the done flag to true.
 * @param server_fd The server file descriptor.
 * @param done The flag to indicate when the server should gracefully stop.
 * @param on_message The callback function to handle incoming messages.
 * @return int The exit status.
 */
int server_loop(int server_fd, const bool *done, message_event on_message);

#endif

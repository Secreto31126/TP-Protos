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
 * @brief Handle a connection event
 *
 * @param client_fd The client file descriptor.
 * @param address The client address information.
 * @return KEEP_CONNECTION_OPEN to keep the connection open.
 * @return CLOSE_CONNECTION to close.
 * @return CONNECTION_ERROR to save to stats and close.
 */
typedef ON_MESSAGE_RESULT (*connection_event)(const int client_fd, struct sockaddr_in address);

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
 * @brief Handle a close event
 * @note The fd is still open to prevent race conditions, but should not be used to read or write.
 *
 * @param client_fd The client file descriptor.
 * @param result The result of the last message handled.
 */
typedef void (*close_event)(const int client_fd, ON_MESSAGE_RESULT result);

/**
 * @brief Initialize a TCP server in non-blocking mode.
 *
 * @param address The server address binded.
 * @return int The server file descriptor, or -1 if an error occurred.
 */
int start_server(struct sockaddr_in *address);
/**
 * @brief The main server loop to handle incoming connections and messages.
 *
 * @note If on_connection rejects the connection, on_close will NOT be triggered.
 * It's expected that on_connection will not allocate resources if it will not connect.
 *
 * @note The server will run until a SIGINT or SIGTERM signal is received, which will set the done flag to true.
 * @param server_fd The server file descriptor.
 * @param done The flag to indicate when the server should gracefully stop.
 * @param on_connection The callback function to handle incoming connections, it may be NULL.
 * @param on_message The callback function to handle incoming messages.
 * @param on_close The callback function to handle closed connections, it may be NULL.
 * @return int The exit status.
 */
int server_loop(int server_fd, const bool *done, connection_event on_connection, message_event on_message, close_event on_close);

/**
 * @brief Asynchronously send a package to a client.
 *
 * @param client_fd The client file descriptor.
 * @param message The message to send.
 * @param length The message length.
 */
void asend(int client_fd, const char *message, size_t length);

#endif

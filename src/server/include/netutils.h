#ifndef NETUTILS_H_CTCyWGhkVt1pazNytqIRptmAi5U
#define NETUTILS_H_CTCyWGhkVt1pazNytqIRptmAi5U

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

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
 * @param server_fd The server that received the connection.
 * @return KEEP_CONNECTION_OPEN to keep the connection open.
 * @return CLOSE_CONNECTION to close.
 * @return CONNECTION_ERROR to save to stats and close.
 */
typedef ON_MESSAGE_RESULT (*connection_event)(const int client_fd, struct sockaddr_in6 address, const int server_fd);

/**
 * @brief Handle a message event
 *
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @param server_fd The server that received the message.
 * @return KEEP_CONNECTION_OPEN to keep the connection open.
 * @return CLOSE_CONNECTION to close.
 * @return CONNECTION_ERROR to save to stats and close.
 */
typedef ON_MESSAGE_RESULT (*message_event)(const int client_fd, const char *body, size_t length, const int server_fd);

/**
 * @brief Handle a close event
 * @note The fd is still open to prevent race conditions, but should not be used to read or write.
 *
 * @param client_fd The client file descriptor.
 * @param result The result of the last message handled.
 * @param server_fd The server that received the message.
 */
typedef void (*close_event)(const int client_fd, ON_MESSAGE_RESULT result, const int server_fd);

/**
 * @brief Handle a finished read and send event
 *
 * @param file The file that has been read.
 * @return int Unused.
 */
typedef int (*read_event)(FILE *file);

/**
 * @brief Initialize a TCP server in non-blocking mode.
 *
 * @param address The server address to bind.
 * @return int The server file descriptor, or -1 if an error occurred.
 */
int start_server(struct sockaddr_in6 *address);
/**
 * @brief Add a server to the poll list.
 * 
 * @note Adding a server after the server_loop produces undefined behavior.
 * (not true, I 100% assert you it would collapse in less than 3 seconds).
 *
 * @param server_fd The server file descriptor.
 * @param address The server address.
 */
void add_server(int server_fd, struct sockaddr_in6 *address);
/**
 * @brief The main server loop to handle incoming connections and messages.
 *
 * @note If on_connection rejects the connection, on_close will NOT be triggered.
 * It's expected that on_connection will not allocate resources if it will not connect.
 *
 * @note The server will run until a SIGINT or SIGTERM signal is received, which will set the done flag to true.
 * @param done The flag to indicate when the server should gracefully stop.
 * @param on_connection The callback function to handle incoming connections, it may be NULL.
 * @param on_message The callback function to handle incoming messages.
 * @param on_close The callback function to handle closed connections, it may be NULL.
 * @return int The exit status.
 */
int server_loop(const bool *done, connection_event on_connection, message_event on_message, close_event on_close);

/**
 * @brief Asynchronously send a package to a client.
 * @note Can only be called during an event.
 *
 * @param client_fd The client file descriptor.
 * @param message The message to send.
 * @param length The message length.
 */
void asend(int client_fd, const char *message, size_t length);
/**
 * @brief Asynchronously read a file and send it to a client.
 * @note Can only be called during an event.
 *
 * @param client_fd The client file descriptor.
 * @param filename The file to read.
 * @param callback The callback after sending the file.
 * @return true If the file was added to the queue.
 * @return false If the file couldn't be opened (maybe it doesn't exists).
 */
bool fasend(int client_fd, FILE *filename, read_event callback);

#endif

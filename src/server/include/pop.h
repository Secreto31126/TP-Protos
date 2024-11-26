#ifndef POP_H
#define POP_H

#include <netutils.h>

#define MAX_POP3_ARG_LENGTH 40
#define MAX_USERNAME_LENGTH MAX_POP3_ARG_LENGTH
#define MAX_PASSWORD_LENGTH 256
#define MAX_POP3_RESPONSE_LENGTH 512
#define MAX_USERS 11

/**
 * @brief Initialize the POP3 server.
 * @note The bytestuffer must take input from stdin and output to stdout.
 *
 * @param bytestuffer The path to the bytestuffer program, defaults to "./dist/bytestuff".
 */
void pop_init(const char *bytestuffer);
/**
 * @brief Finalize the POP3 server.
 */
void pop_stop();

/**
 * @brief Handle a POP3 connection. Populates the client connection with the initial state.
 * @note Implementation of connection_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param address The client address information.
 * @param port The port the connection was received on.
 * @return KEEP_CONNECTION_OPEN if the connection is handled as expected.
 * @return CONNECTION_ERROR if an error occurred.
 */
ON_MESSAGE_RESULT handle_pop_connect(int client_fd, struct sockaddr_in address, const short port);

/**
 * @brief Handles a POP3 message, which may contain multiple instructions separated by \\r\\n.
 * It will split the message into individual commands and handle them in order.
 * @note Implementation of message_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @param port The port the message was received on.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
ON_MESSAGE_RESULT handle_pop_message(int client_fd, const char *body, size_t length, const short port);

/**
 * @brief Free the resources associated with a client connection.
 * @note This method MUST NOT trigger any UPDATE state logic, as it might be called
 * when the client disconnects unexpectedly or is still in the AUTHORIZATION state.
 * @note Implementation of close_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param result The result of the last message handled.
 * @param port The port the connection was received on.
 */
void handle_pop_close(int client_fd, ON_MESSAGE_RESULT result, const short port);

#endif

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
 * @note The transformer must take a filename as last argument.
 *
 * @param dir The mail directory, defaults to "./dist/mail".
 * @param transformer The transformer to use for the mail messages, defaults to "cat".
 * @param bytestuffer The path to the bytestuffer program, defaults to "./dist/bytestuff".
 */
void pop_init(const char *dir, const char *transformer, const char *bytestuffer);

/**
 * @brief Handle a POP3 connection. Populates the client connection with the initial state.
 * @note Implementation of connection_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param address The client address information.
 * @return KEEP_CONNECTION_OPEN if the connection is handled as expected.
 * @return CONNECTION_ERROR if an error occurred.
 */
ON_MESSAGE_RESULT handle_pop_connect(int client_fd, struct sockaddr_in address);

/**
 * @brief Handles a POP3 message, which may contain multiple instructions separated by \\r\\n.
 * It will split the message into individual commands and handle them in order.
 * @note Implementation of message_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
ON_MESSAGE_RESULT handle_pop_message(int client_fd, const char *body, size_t length);

/**
 * @brief Free the resources associated with a client connection.
 * @note This method MUST NOT trigger any UPDATE state logic, as it might be called
 * when the client disconnects unexpectedly or is still in the AUTHORIZATION state.
 * @note Implementation of close_event handler.
 *
 * @param client_fd The client file descriptor.
 * @param result The result of the last message handled.
 */
void handle_pop_close(int client_fd, ON_MESSAGE_RESULT result);

#endif

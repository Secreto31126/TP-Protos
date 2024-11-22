#include <netutils.h>

#include <errno.h>
#include <fcntl.h>
#include <logger.h>
#include <magic.h>
#include <poll.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define CLOSE_SOCKET(fds, nfds, i) \
    close(fds[i].fd);              \
    fds[i--] = fds[--nfds];

typedef struct DataList
{
    struct Data *first;
    struct Data *last;
} DataList;

/**
 * @brief This stores clients pending messages
 */
typedef struct Data
{
    enum
    {
        /**
         * @brief Data with content that can be transmited to a client
         */
        RAW_DATA,
        /**
         * @brief A branch of messages from the master queue,
         * asserting messages order without blocking the main list.
         */
        MESSAGE_SPLITTER,
        /**
         * @brief Indicates that the connection can be closed gracefully
         */
        ESC
    } type;
    union
    {
        struct
        {
            /**
             * @brief Please remember to free me later
             */
            void *ptr;
            /**
             * @brief Please never free me
             */
            char *data;
            size_t length;
        } raw;
        struct
        {
            int fd;
            DataList messages;
            /**
             * @brief The next splitter in the linked list of splitters
             */
            struct Data *next;
        } splitter;
    };
    /**
     * @brief The next node in the linked list of nodes
     */
    struct Data *next;
} Data;

typedef struct DataHeader
{
    enum DataHeaderType
    {
        FD_SOCKET,
        FD_FILE
    } type;
    union
    {
        struct
        {
            DataList messages;
            DataList splitters;
        };
        struct
        {
            read_event read_callback;
            int client_fd;
            FILE *file;
        };
    };
} DataHeader;

/**
 * @brief Append to a data list a new message
 *
 * @note Must be called with the fds_mutex held
 *
 * @param list The DataList to append to.
 * @param client_fd The client file descriptor.
 * @param message The message to send.
 * @param length The message length.
 */
static void iasend(DataList *list, int client_fd, const char *message, size_t length);
/**
 * @brief Sends the pending messages to the client
 *
 * @note Must be called with the fds_mutex held
 *
 * @param list The DataList of pending messages.
 * @param client_fd The client file descriptor.
 * @param fds_index The poll array index of the client.
 * @param empty_node Indicates if the node must be removed from the list. The root call must send NULL.
 * @return KEEP_CONNECTION_OPEN to keep the connection open
 * @return CLOSE_CONNECTION to close the connection (an ESC node was found)
 * @return CONNECTION_ERROR if an error happened sending the message
 */
static ON_MESSAGE_RESULT time_to_send(DataList *list, int client_fd, int fds_index, bool *empty_node);
/**
 * @brief Sends a buffer of a file to a client
 *
 * @note Must be called with the fds_mutex held
 *
 * @param client_fd The client file descriptor.
 * @param file The reading file.
 * @return true Keep reading the file
 * @return false Close the file and remove it from the poll
 */
static bool time_to_read(int client_fd, FILE *file);
/**
 * @brief Gracefully stop a socket connection,
 * disabling the POLLIN event and appending an ESC node
 * if the list still contains pending messages
 *
 * @param list The DataList of pending messages.
 * @param client_fd The client file descriptor.
 * @param fds_index The poll array index of the client.
 * @return true The connection can be closed immediately
 * @return false The connection must wait until all messages are sent
 */
static bool finish_transmition(DataList *list, int client_fd, int fds_index);
/**
 * @brief Recursively deallocate the memory of a Data linked list
 * 
 * @param data The first node of the linked list
 */
static void free_data(Data *data);

// Array to hold client sockets and poll event types
static struct pollfd fds[MAGIC_NUMBER];
static sem_t fds_mutex;
static int nfds = 0;

// Array to hold pending messages or files
static DataHeader pending[MAGIC_NUMBER];

int start_server(struct sockaddr_in *address)
{
    int server_fd;
    int opt = 1;

    // Initialize the pending mutex
    if (sem_init(&fds_mutex, 0, 1) < 0)
    {
        perror("sem_init failed");
        return -1;
    }

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("socket failed");
        return -1;
    }

    // Set the server socket settings
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
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

static ON_MESSAGE_RESULT keep_alive_noop()
{
    return KEEP_CONNECTION_OPEN;
}

static void noop()
{
}

int server_loop(int server_fd, const bool *done, connection_event on_connection, message_event on_message, close_event on_close)
{
    struct sockaddr_in address;
    int new_socket, addrlen = sizeof(address);

    on_connection = on_connection ? on_connection : keep_alive_noop;
    on_close = on_close ? on_close : noop;

    sem_wait(&fds_mutex);

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds = 1;

    sem_post(&fds_mutex);

    while (!*done)
    {
        int activity = poll(fds, nfds, -1);
        if (activity < 0)
        {
            perror("poll error");
            return EXIT_FAILURE;
        }

        sem_wait(&fds_mutex);

        // Check for new connections on the server socket
        if (fds[0].revents & POLLIN)
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                return EXIT_FAILURE;
            }

            LOG("New connection: socket fd %s:%d\n", inet_ntoa(address.sin_addr), new_socket);

            free_data(pending[new_socket].messages.first);
            pending[new_socket].type = FD_SOCKET;
            pending[new_socket].messages.first = NULL;
            pending[new_socket].messages.last = NULL;

            // Add new socket to fds array
            fds[nfds].fd = new_socket;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;

            ON_MESSAGE_RESULT result = on_connection(new_socket, address);

            if (result != KEEP_CONNECTION_OPEN)
            {
                LOG("Closing connection: socket fd %d\n", new_socket);

                if (result == CONNECTION_ERROR)
                {
                    // TODO: Real stats
                    LOG("Error handling message\n");

                    close(new_socket);
                    nfds--;
                }
                else if (finish_transmition(&pending[new_socket].messages, new_socket, nfds - 1))
                {
                    close(new_socket);
                    nfds--;
                }
            }
        }

        // Check each fd for activity
        for (int i = 1; i < nfds; i++)
        {
            int fd = fds[i].fd;

            if (fds[i].revents & POLLIN)
            {
                if (pending[fd].type == FD_FILE)
                {
                    bool finished = !time_to_read(pending[fd].client_fd, pending[fd].file);

                    if (finished)
                    {
                        pending[fd].read_callback(pending[fd].file, fd);
                    }
                }

                if (pending[fd].type == FD_SOCKET)
                {
                    char buffer[1024] = {0};
                    int len = recv(fd, buffer, sizeof(buffer), 0);

                    // Connection closed or error, remove from poll
                    if (len <= 0)
                    {
                        LOG("Client disconnected: socket fd %d\n", fd);

                        Data *splitter = pending[fd].splitters.first;
                        while (splitter)
                        {
                            pending[fd].read_callback(pending[fd].file, -1);
                        }

                        free_data(pending[fd].messages.first);

                        on_close(fd, CONNECTION_ERROR);
                        CLOSE_SOCKET(fds, nfds, i);
                        continue;
                    }

                    LOG("Received from client %d (%d bytes): %s\n", fd, len, buffer);

                    ON_MESSAGE_RESULT result = on_message(fd, buffer, len);

                    if (result != KEEP_CONNECTION_OPEN)
                    {
                        if (result == CONNECTION_ERROR)
                        {
                            // TODO: Real stats
                            LOG("Error handling message\n");
                        }
                        else if (!finish_transmition(&pending[fd].messages, fd, i))
                        {
                            continue;
                        }

                        LOG("Closing connection: socket fd %d\n", fd);

                        Data *splitter = pending[fd].splitters.first;
                        while (splitter)
                        {
                            pending[fd].read_callback(pending[fd].file, -1);
                        }

                        free_data(pending[fd].messages.first);

                        on_close(fd, result);
                        CLOSE_SOCKET(fds, nfds, i);
                        continue;
                    }
                }
            }

            if (fds[i].revents & POLLOUT)
            {
                DataHeader *header = pending + fd;

                if (header->type != FD_SOCKET)
                {
                    fds[i].events &= ~POLLOUT;
                    continue;
                }

                ON_MESSAGE_RESULT result = time_to_send(&header->messages, fd, i, NULL);

                if (result != KEEP_CONNECTION_OPEN)
                {
                    if (result == CONNECTION_ERROR)
                    {
                        // TODO : Real stats
                        LOG("Error handling message\n");
                    }

                    LOG("Closing connection: socket fd %d\n", fd);

                    on_close(fd, result);
                    CLOSE_SOCKET(fds, nfds, i);
                }
            }
        }

        sem_post(&fds_mutex);
    }

    return EXIT_SUCCESS;
}

void asend(int client_fd, const char *message, size_t length)
{
    iasend(&pending[client_fd].messages, client_fd, message, length);
}

static ON_MESSAGE_RESULT time_to_send(DataList *list, int client_fd, int fds_index, bool *empty_node)
{
    Data *data = list->first;

    if (!data)
    {
        if (!empty_node)
        {
            fds[fds_index].events &= ~POLLOUT;
            return KEEP_CONNECTION_OPEN;
        }

        *empty_node = true;
        return KEEP_CONNECTION_OPEN;
    }

    if (data->type == ESC)
    {
        free_data(data);
        list->first = NULL;
        list->last = NULL;
        return CLOSE_CONNECTION;
    }

    if (data->type == MESSAGE_SPLITTER)
    {
        bool empty_splitter = false;
        ON_MESSAGE_RESULT result = time_to_send(&data->splitter.messages, client_fd, -1, &empty_splitter);

        if (empty_splitter && data->splitter.fd < 0)
        {
            Data *next = data->next;

            free_data(data);

            list->first = next;

            if (!next)
            {
                list->last = NULL;

                if (!empty_node)
                {
                    fds[fds_index].events &= ~POLLOUT;
                }
                else
                {
                    *empty_node = true;
                }
            }
            else
            {
                return time_to_send(list, client_fd, fds_index, empty_node);
            }
        }
        else if (empty_splitter)
        {
            // Disable POLLOUT if no more messages in splitter but it's still open
            fds[fds_index].events &= ~POLLOUT;
        }

        return result;
    }

    char *message = data->raw.data;
    size_t length = data->raw.length;

    size_t sent = send(client_fd, message, length, 0);
    if (sent < 0)
    {
        free_data(data);
        list->first = NULL;
        list->last = NULL;
        return CONNECTION_ERROR;
    }

    if (sent < length)
    {
        data->raw.data = message + sent;
        data->raw.length = length - sent;
        return KEEP_CONNECTION_OPEN;
    }

    free(data->raw.ptr);

    Data *next = data->next;

    free(data);

    list->first = next;

    if (!next)
    {
        list->last = NULL;
        fds[fds_index].events &= ~POLLOUT;
    }

    return KEEP_CONNECTION_OPEN;
}

bool fasend(int client_fd, FILE *file, read_event callback)
{
    Data *splitter = malloc(sizeof(Data));

    if (!splitter)
    {
        return false;
    }

    int file_fd = fileno_unlocked(file);

    pending[file_fd].type = FD_FILE;
    pending[file_fd].read_callback = callback;
    pending[file_fd].client_fd = client_fd;
    pending[file_fd].file = file;

    fds[nfds].fd = file_fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    nfds++;

    splitter->type = MESSAGE_SPLITTER;
    splitter->splitter.fd = file_fd;
    splitter->splitter.messages.first = NULL;
    splitter->splitter.messages.last = NULL;
    splitter->splitter.next = NULL;

    DataHeader *header = pending + client_fd;
    bool empty = !header->messages.first;

    if (empty)
    {
        header->messages.first = splitter;
    }
    else
    {
        header->messages.last->next = splitter;
    }

    header->messages.last = splitter;

    bool empty_splitters = !header->splitters.first;

    if (empty_splitters)
    {
        header->splitters.first = splitter;
    }
    else
    {
        header->splitters.last->splitter.next = splitter;
    }

    header->splitters.last = splitter;

    return true;
}

static bool time_to_read(int client_fd, FILE *file)
{
    Data *splitter = pending[client_fd].splitters.first;
    while (splitter && splitter->splitter.fd != fileno_unlocked(file))
    {
        splitter = splitter->splitter.next;
    }

    if (!splitter)
    {
        return false;
    }

    char message[512];
    size_t length = fread(message, sizeof(char), sizeof(message), file);

    if (length)
    {
        iasend(&splitter->splitter.messages, client_fd, message, length);
    }

    if (feof_unlocked(file) || ferror_unlocked(file))
    {
        splitter->splitter.fd = -1;
        return false;
    }

    return true;
}

static void iasend(DataList *list, int client_fd, const char *message, size_t length)
{
    Data *data = malloc(sizeof(Data));

    if (!data)
    {
        return;
    }

    data->type = RAW_DATA;
    data->raw.ptr = malloc(length);

    if (!data->raw.ptr)
    {
        free(data);
        return;
    }

    memcpy(data->raw.ptr, message, length);

    data->raw.data = data->raw.ptr;
    data->raw.length = length;

    bool empty = !list->first;

    if (empty)
    {
        list->first = data;
    }
    else
    {
        list->last->next = data;
    }

    list->last = data;

    if (empty)
    {
        for (size_t i = 1; i < nfds; i++)
        {
            if (fds[i].fd == client_fd)
            {
                fds[i].events |= POLLOUT;
                break;
            }
        }
    }
}

static bool finish_transmition(DataList *list, int client_fd, int fds_index)
{
    fds[fds_index].events &= ~POLLIN;

    bool empty = !list->first;

    if (empty)
    {
        return true;
    }

    Data *data = malloc(sizeof(Data));

    if (!data)
    {
        // Bad luck :]
        return false;
    }

    data->type = ESC;

    list->last->next = data;
    list->last = data;

    return false;
}

static void free_data(Data *data)
{
    if (!data)
    {
        return;
    }

    if (data->next)
    {
        free_data(data->next);
    }

    if (data->type == RAW_DATA)
    {
        free(data->raw.ptr);
    }
    else if (data->type == MESSAGE_SPLITTER)
    {
        free_data(data->splitter.messages.first);
    }

    free(data);
}

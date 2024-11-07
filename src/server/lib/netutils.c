#include <netutils.h>

#include <fcntl.h>
#include <logger.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define CLOSE_SOCKET(fds, nfds, i) \
    close(fds[i].fd);              \
    fds[i--] = fds[--nfds];

int start_server(struct sockaddr_in *address, int port)
{
    int server_fd;
    int opt = 1;

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(port);

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

    // Array to hold client sockets and poll event types
    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    while (!*done)
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

            LOG("New connection: socket fd %s:%d\n", inet_ntoa(address.sin_addr), new_socket);

            ON_MESSAGE_RESULT result = on_connection(new_socket, address);

            if (result != KEEP_CONNECTION_OPEN)
            {
                if (result == CONNECTION_ERROR)
                {
                    // TODO: Real stats
                    LOG("Error handling message\n");
                }

                LOG("Closing connection: socket fd %d\n", new_socket);
                close(new_socket);
            } else {
                // Add new socket to fds array
                fds[nfds].fd = new_socket;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
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
            int len = read(fds[i].fd, buffer, sizeof(buffer));

            // Connection closed or error, remove from poll
            if (len <= 0)
            {
                LOG("Client disconnected: socket fd %d\n", fds[i].fd);

                on_close(fds[i].fd);
                CLOSE_SOCKET(fds, nfds, i);
                continue;
            }

            LOG("Received from client %d (%d bytes): %s\n", fds[i].fd, len, buffer);

            ON_MESSAGE_RESULT result = on_message(fds[i].fd, buffer, len);

            if (result != KEEP_CONNECTION_OPEN)
            {
                if (result == CONNECTION_ERROR)
                {
                    // TODO: Real stats
                    LOG("Error handling message\n");
                }

                LOG("Closing connection: socket fd %d\n", fds[i].fd);

                on_close(fds[i].fd);
                CLOSE_SOCKET(fds, nfds, i);
            }
        }
    }

    return EXIT_SUCCESS;
}

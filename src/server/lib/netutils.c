#include <netutils.h>

#include <fcntl.h>
#include <logger.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

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

int server_loop(int server_fd, const bool *done, message_event on_message)
{
    struct sockaddr_in address;
    int new_socket, addrlen = sizeof(address);

    // Array to hold client sockets and poll event types
    struct pollfd fds[MAX_CLIENTS];
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

            if (result != KEEP_CONNECTION_OPEN)
            {
                if (result == CONNECTION_ERROR)
                {
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

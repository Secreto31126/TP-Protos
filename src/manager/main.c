#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h> // socket
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include <signal.h>

#define DEFAULT_PORT 4321
#define DEFAULT_IP "127.0.0.1"
#define BLOCK 1024
#define OK "+OK"
#define ERROR "-ERR"

static int done = 0;

void invalid_arguments(int index, char *argv[])
{
    printf("Unknown argument: %s\n", argv[index]);
    printf("Known flags are:\n-p <port>\n-i <ip address>\n");
}

void parse_response(char *response)
{
    if (!strncmp(OK, response, sizeof(OK) - 1))
        printf("\033[0;32m");
    else if (!strncmp(ERROR, response, sizeof(ERROR) - 1))
        printf("\033[0;31m");
    printf("%s\033[0m\n", response);
}

int setup(int port, char *ip)
{
    int sockfd;
    struct sockaddr_in server_addr;

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid IP address");
        close(sockfd);
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection to the server failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int read_arguments(int argc, char *argv[], int *ret_port, char **ret_ip)
{
    *ret_port = DEFAULT_PORT;
    *ret_ip = DEFAULT_IP;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-p"))
        {
            if (argc < i + 1)
            {
                invalid_arguments(i, argv);
                return -1;
            }
            *ret_port = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-i"))
        {
            if (argc < i + 1)
            {
                invalid_arguments(i, argv);
                return -1;
            }
            *ret_ip = argv[++i];
        }
        else
        {
            invalid_arguments(i, argv);
            return -1;
        }
    }
    return 0;
}

// must have at least twice as much space allocated in response buffer as the length of the message
void parse_message(char *message, char *response_buffer)
{
    int added = 0, carriage_return_flag = 0;
    for (int i = 0; message[i]; i++)
    {
        if (message[i] == '\n' && !carriage_return_flag)
        {
            response_buffer[i + added] = '\r';
            added++;
            i--;
            carriage_return_flag = 1;
        }
        else
        {
            carriage_return_flag = message[i] == '\r';
            response_buffer[i + added] = message[i];
        }
    }
}

int application_loop(int sockfd)
{
    struct pollfd fds[2] = {{.fd = STDIN_FILENO, .events = POLLIN}, {.fd = sockfd, .events = POLLIN}};
    int ret;
    char buffer[BLOCK];
    while (!done)
    {
        ret = poll(fds, 2, -1);

        if (ret < 0)
        {
            perror("Poll failed");
            return 1;
        }

        if (fds[0].revents & POLLIN)
        {
            fgets(buffer, BLOCK, stdin);
            char send_buffer[BLOCK * 2] = {0};
            parse_message(buffer, send_buffer);
            // Send message to the server
            if (send(sockfd, send_buffer, strlen(send_buffer), 0) < 0)
            {
                perror("Failed to send message");
                close(sockfd);
                return 1;
            }
        }

        if (fds[1].revents & POLLIN)
        {
            size_t res_len = 0;
            if ((res_len = recv(sockfd, buffer, BLOCK - 1, 0)) < 0)
            {
                perror("Failed to receive response");
                return 1;
            }

            if (!res_len)
            {
                printf("Connection closed by server\n");
                return 0;
            }

            else
            {
                buffer[res_len] = 0;
                parse_response(buffer);
            }
        }
        // Receive response from the server
    }
    return 0;
}

static void sigterm_handler(const int signal)
{
    done = 1;
}

static void sigint_handler(const int signal)
{
    sigterm_handler(signal);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);

    int port;
    char *ip;

    if (read_arguments(argc, argv, &port, &ip) < 0)
        return 1;
    printf("Connecting to the server at %s:%d...\n", ip, port);
    int sockfd = setup(port, ip);

    if (sockfd < 0)
        return 1;

    printf("Connected to the server at %s:%d.\n\n", ip, port);
    application_loop(sockfd);

    close(sockfd);
    printf("Exiting.\n");
}

#include <common_config.h>

#include <limits.h>
#include <stdlib.h>

bool safe_username(const char *username)
{
    if (!*username || *username == '.')
    {
        return false;
    }

    const char *n = username;
    while (*n)
    {
        if (*n == '/' || ++n - username /* length */ > MAX_USERNAME_LENGTH)
        {
            return false;
        }
    }

    return true;
}

char set_address(const char *input, struct sockaddr_in6 *address)
{
    int aux = inet_pton(AF_INET6, input, &(address->sin6_addr));
    return aux < 1 ? 0 : 1;
}

char set_port(const char *input, struct sockaddr_in6 *address)
{
    __u_long port = strtol(input, NULL, 10);
    if (port <= 0 || port > USHRT_MAX)
    {
        return 1;
    }
    address->sin6_port = htons((__u_short)port);
    return 0;
}
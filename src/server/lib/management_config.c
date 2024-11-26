#include <management_config.h>
#include <stdlib.h>
#include <string.h>

static struct sockaddr_in _management_addr = {
    .sin_family = AF_INET,
    .sin_port = MANAGER_DEFAULT_PORT,
    .sin_addr.s_addr = INADDR_ANY
    };

static unsigned int _admin_count = 0;
static Admin _admins[MAX_ADMINS] = {0};

char *get_version()
{
    return VERSION;
}

struct sockaddr_in get_manager_adport()
{
    return _management_addr;
}

void set_manager_port(const char *new_port) {
    set_port(new_port, &_management_addr);
}

char set_management_address(const char *new_addr)
{
    return set_address(new_addr, &_management_addr);
}

char set_management_port(const char *new_port)
{
    return set_port(new_port, &_management_addr);
}

Admin *get_admin(const char *username)
{
    if (!safe_username(username))
    {
        return NULL;
    }

    size_t i = 0;
    for (; i < _admin_count; i++)
    {
        if (strncmp(username, _admins[i].username, MAX_USERNAME_LENGTH) == 0)
        {
            break;
        }
    }
    if (i < _admin_count)
    {
        return &(_admins[i]);
    }
    return NULL;
}

char add_admin(const char *username, const char *password)
{
    if (!safe_username(username) || *password == '\0' || strlen(password) > MAX_PASSWORD_LENGTH)
    {
        return 1;
    }

    size_t i = 0;
    for (; i < _admin_count; i++)
    {
        if (strncmp(username, _admins[i].username, MAX_USERNAME_LENGTH) == 0)
        {
            break;
        }
    }
    if (i < _admin_count)
    {
        strncpy(_admins[i].password, password, MAX_PASSWORD_LENGTH);
    }
    else if (_admin_count < MAX_USERS)
    {
        strncpy(_admins[_admin_count].password, password, MAX_PASSWORD_LENGTH);
        strncpy(_admins[_admin_count].username, username, MAX_USERNAME_LENGTH);

        _admin_count++;
    }
    else
    {
        return 2;
    }
    return 0;
}

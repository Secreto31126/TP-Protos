#include <pop_config.h>
#include <dirent.h>
#include <common_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *const _default_mail_dir = "./dist/mail";
static char *_mail_dir = _default_mail_dir;

static char *const _default_transformer = "cat";
static char *_transformer = _default_transformer;

static unsigned int _user_count = 0;
static User _users[MAX_USERS] = {0};

static struct sockaddr_in6 _pop_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = POP_DEFAULT_PORT,
    .sin6_addr = IN6ADDR_ANY_INIT
    };

static void create_user_maildir(const char *base_maildir, const char *username)
{
    size_t maildir_len = strlen(base_maildir);

    char *user_base_dir = calloc(1, maildir_len + MAX_USERNAME_LENGTH + 2);
    strncpy(user_base_dir, base_maildir, MAX_POP3_ARG_LENGTH);
    strncat(user_base_dir, "/", 2);
    strncat(user_base_dir, username, MAX_USERNAME_LENGTH + 1);

    char *user_new_dir = calloc(1, maildir_len + MAX_USERNAME_LENGTH + 5);
    char *user_cur_dir = calloc(1, maildir_len + MAX_USERNAME_LENGTH + 5);
    char *user_tmp_dir = calloc(1, maildir_len + MAX_USERNAME_LENGTH + 5);

    snprintf(user_new_dir, maildir_len + MAX_USERNAME_LENGTH + 5, "%s/new", user_base_dir);
    snprintf(user_cur_dir, maildir_len + MAX_USERNAME_LENGTH + 5, "%s/cur", user_base_dir);
    snprintf(user_tmp_dir, maildir_len + MAX_USERNAME_LENGTH + 5, "%s/tmp", user_base_dir);

    if (access(user_base_dir, F_OK) == -1)
    {
        mkdir(user_base_dir, S_IRWXU);
    }
    if (access(user_new_dir, F_OK) == -1)
    {
        mkdir(user_new_dir, S_IRWXU);
    }
    if (access(user_cur_dir, F_OK) == -1)
    {
        mkdir(user_cur_dir, S_IRWXU);
    }
    if (access(user_tmp_dir, F_OK) == -1)
    {
        mkdir(user_tmp_dir, S_IRWXU);
    }

    free(user_base_dir);
    free(user_new_dir);
    free(user_cur_dir);
    free(user_tmp_dir);
}

/**PUBLIC FUNCTIONS */

char *get_maildir()
{
    return _mail_dir;
}

struct sockaddr_in6 get_pop_adport()
{
    return _pop_addr;
}

char *get_transformer()
{
    return _transformer;
}

size_t get_users_arr(const User **users)
{
    *users = _users;
    return _user_count;
}

User *get_user(const char *username)
{
    if (!safe_username(username))
    {
        return NULL;
    }

    size_t i = 0;
    for (; i < _user_count; i++)
    {
        if (strncmp(username, _users[i].username, MAX_USERNAME_LENGTH) == 0)
        {
            break;
        }
    }
    if (i < _user_count)
    {
        return &(_users[i]);
    }
    return NULL;
}

char set_pop_address(const char *new_addr)
{
    return set_address(new_addr, &_pop_addr);
}

char set_pop_port(const char *new_port)
{
    return set_port(new_port, &_pop_addr);
}

void set_maildir(const char *new_maildir)
{
    if (_mail_dir == new_maildir)
    {
        return;
    }

    if (access(new_maildir, F_OK) == -1)
    {
        mkdir(new_maildir, S_IRWXU);
    }

    for (size_t i = 0; i < _user_count; i++)
    {
        create_user_maildir(new_maildir, _users[i].username);
    }

    if (_mail_dir && _mail_dir != _default_mail_dir)
    {
        free(_mail_dir);
    }

    _mail_dir = strdup(new_maildir);
}

void set_transformer(const char *transformer)
{
    if (_transformer == transformer)
    {
        return;
    }

    if (_transformer && _transformer != _default_transformer)
    {
        free(_transformer);
    }

    _transformer = strdup(transformer);
}

char set_user(const char *username, const char *password)
{
    if (!safe_username(username) || *password == '\0' || strlen(password) > MAX_PASSWORD_LENGTH)
    {
        return 1;
    }

    size_t i = 0;
    for (; i < _user_count; i++)
    {
        if (strncmp(username, _users[i].username, MAX_USERNAME_LENGTH) == 0)
        {
            break;
        }
    }
    if (i < _user_count)
    {
        strncpy(_users[i].password, password, MAX_PASSWORD_LENGTH);
    }
    else if (_user_count < MAX_USERS)
    {
        _users[_user_count].locked = false;
        strncpy(_users[_user_count].password, password, MAX_PASSWORD_LENGTH);
        strncpy(_users[_user_count].username, username, MAX_USERNAME_LENGTH);

        create_user_maildir(_mail_dir, username);

        _user_count++;
    }
    else
    {
        return 2;
    }
    return 0;
}

char delete_user(const char *username)
{
    User *user = get_user(username);
    if (user == NULL)
    {
        return 1;
    }

    if (user->locked)
    {
        return 2; // The user is logged in so it can't be deleted
    }

    user->locked = _users[_user_count - 1].locked;
    strcpy(user->password, _users[_user_count - 1].password);
    strcpy(user->username, _users[_user_count - 1].username);
    _user_count--;
    return 0;
}

char set_user_lock(const char *username)
{
    User *user = get_user(username);

    if (user == NULL || user->locked)
    {
        return 1;
    }

    user->locked = true;
    return 0;
}

char unset_user_lock(const char *username)
{
    User *user = get_user(username);

    if (user == NULL)
    {
        return 1;
    }

    user->locked = false;
    return 0;
}

void shutdown_pop_configs()
{
    if (_mail_dir && _mail_dir != _default_mail_dir)
    {
        free(_mail_dir);
    }
    if (_transformer && _transformer != _default_transformer)
    {
        free(_transformer);
    }
}

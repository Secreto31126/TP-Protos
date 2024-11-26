#include <pop.h>

#include <ctype.h>
#include <dirent.h>
#include <logger.h>
#include <magic.h>
#include <math.h>
#include <pthread.h>
#include <pop_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CONNECTION_BUFFER_SIZE 1024

#define POP3_OK "+OK"
#define POP3_ERR "-ERR"
#define POP3_ENTER "\r\n"

// Remember to include a space on the left of the message
#define OK_RESPONSE(m) POP3_OK m POP3_ENTER
// Remember to include a space on the left of the message
#define ERR_RESPONSE(m) POP3_ERR m POP3_ENTER

#define POP_MIN(x) fmin((x), MAX_POP3_RESPONSE_LENGTH)

#define MAX_CLIENT_MAILS 0x1000

#define MAX_ADMIN_CONNECTIONS 10

/**
 * @brief The mail information for a client connection.
 */
typedef struct Mailfile
{
    /**
     * @brief The mail unique ID and filename
     */
    char uid[256];
    /**
     * @brief If the mail is marked for deletion by the client
     * @note The mails are not deleted until the client enters the UPDATE state
     */
    bool deleted;
    /**
     * @brief The mail size in bytes
     */
    size_t size;
} Mailfile;

/**
 * @brief The client connection information.
 */
typedef struct Connection
{
    /**
     * @brief The client buffer for incomplete commands
     */
    char buffer[CONNECTION_BUFFER_SIZE];
    /**
     * @brief The client username
     * @note This fields MUST ALWAYS contain safe usernames (see safe_username())
     * @note If empty, the client should send the USER command
     */
    char username[MAX_USERNAME_LENGTH + 1];
    /**
     * @brief If the client is authenticated (TRANSACTION state)
     */
    bool authenticated;
    /**
     * @brief If the client mails need to be updated (UPDATE state)
     */
    bool update;
    /**
     * @brief The client mails (loaded after the AUTHORIZATION state)
     */
    Mailfile *mails;
    /**
     * @brief The number of mails in the client mails array
     */
    size_t mail_count;
} Connection;

static Connection *connections[MAGIC_NUMBER] = {NULL};

/**
 * @brief The path to the bytestuffer program.
 */
static const char *stuffer;

static int active_managers = 0;

void pop_init(const char *bytestuffer)
{
    stuffer = bytestuffer ? bytestuffer : "./dist/bytestuff";
}

void pop_stop()
{
    shutdown_pop_configs();
}

/**
 * @brief Parses a POP3 input, replacing spaces with null terminators.
 * Changes the POP3 command to UPPER CASE. Stops at a null terminator.
 *
 * @note The input command is modified in place.
 *
 * @param cmd The input command.
 * @return int The number of arguments in the input (excluding the POP3 command).
 */
static int parse_pop_cmd(char *cmd)
{
    int argc = 0;
    while (*cmd)
    {
        if (*cmd == ' ')
        {
            argc++;
            *cmd = 0;
        }

        if (!argc)
        {
            *cmd = toupper(*cmd);
        }

        cmd++;
    }

    return argc;
}

/**
 * @brief Validate if a user exists in the mail directory.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The user exists.
 * @return false The user does not exist.
 */
static bool user_exists(const char *username)
{
    return get_user(username) != NULL;
}

/**
 * @brief Validate if a user mail directory isn't locked.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The user directory is locked.
 * @return false The user directory is not locked or doesn't exists.
 */
static bool user_locked(const char *username)
{
    User *user = get_user(username);
    return user && user->locked;
}

/**
 * @brief Get a maildir lock.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The lock was successfully set.
 * @return false The lock could not be set.
 */
static bool set_lock(const char *username)
{
    return set_user_lock(username) == 0;
}

/**
 * @brief Remove a maildir lock.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The lock was successfully removed.
 * @return false The lock could not be removed (invalid user).
 */
static bool remove_lock(const char *username)
{
    return unset_user_lock(username) == 0;
}

/**
 * @brief Validate a user password.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The username (NULL terminated).
 * @param pass The input password (NULL terminated).
 * @return true The password is correct.
 * @return false The password is incorrect (or failed to read the password file).
 */
static bool pass_valid(const char *username, const char *pass)
{
    User *user = get_user(username);

    if (!user)
    {
        return false;
    }

    return !strcmp(user->password, pass);
}

/**
 * @brief Set the user emails in the client connection.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The username (NULL terminated).
 * @param client The client connection.
 * @return true The mails were successfully loaded.
 * @return false The mails could not be loaded.
 */
static bool set_user_mails(const char *username, Connection *client)
{
    char *maildir = get_maildir();

    char new_path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/new")];
    snprintf(new_path, sizeof(new_path), "%s/%s/new", maildir, username);

    char cur_path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/cur")];
    snprintf(cur_path, sizeof(cur_path), "%s/%s/cur", maildir, username);

    DIR *new_dir = opendir(new_path);
    if (!new_dir)
    {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(new_dir)))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        {
            continue;
        }

        char new_filepath[strlen(new_path) + sizeof("/") + sizeof(entry->d_name)];
        snprintf(new_filepath, sizeof(new_filepath), "%s/%s", new_path, entry->d_name);

        char cur_filepath[strlen(cur_path) + sizeof("/") + sizeof(entry->d_name) + sizeof(":2,S")];
        snprintf(cur_filepath, sizeof(cur_filepath), "%s/%s:2,S", cur_path, entry->d_name);

        if (rename(new_filepath, cur_filepath))
        {
            closedir(new_dir);
            return false;
        }
    }

    closedir(new_dir);

    DIR *dir = opendir(cur_path);
    if (!dir)
    {
        return false;
    }

    size_t allocated = 4;
    client->mail_count = 0;
    client->mails = malloc(allocated * sizeof(Mailfile));

    while ((entry = readdir(dir)))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        {
            continue;
        }

        size_t i = client->mail_count;

        memcpy(client->mails[i].uid, entry->d_name, sizeof(client->mails[i].uid));
        client->mails[i].uid[sizeof(client->mails[i].uid) - 1] = 0;

        client->mails[i].deleted = false;

        char filepath[strlen(cur_path) + sizeof("/") + sizeof(client->mails[i].uid)];
        snprintf(filepath, sizeof(filepath), "%s/%s", cur_path, client->mails[i].uid);

        struct stat buf;
        if (stat(filepath, &buf) < 0)
        {
            closedir(dir);
            return false;
        }

        client->mails[i].size = buf.st_size;

        if (++client->mail_count >= MAX_CLIENT_MAILS)
        {
            break;
        }

        if (client->mail_count >= allocated)
        {
            allocated *= 2;
            client->mails = realloc(client->mails, allocated * sizeof(Mailfile));
        }
    }

    closedir(dir);

    return true;
}

/**
 * @brief Handles a USER command. If the username is valid, it is stored in the client connection.
 * If not, the username is cleared.
 *
 * @param client The client connection.
 * @param username The input username (NULL terminated).
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_user(Connection *client, const char *username, char **response)
{
    strncpy(client->username, username, sizeof(client->username) - 1);
    client->username[sizeof(client->username) - 1] = 0;

    *response = OK_RESPONSE();
    return sizeof(OK_RESPONSE()) - 1;
}

/**
 * @brief Handles a PASS command. If the password is correct and the lock isn't active,
 * the client is authenticated, the lock is raised and the user mails are loaded.
 * If not, the username is cleared, the client is not authenticated, and the lock is not raised.
 *
 * @param client The client connection.
 * @param pass The input password (NULL terminated).
 * @param response The response to send back to the client.
 * @param is_manager If the client is a manager.
 * @return size_t The length of the response.
 */
static size_t handle_pass(Connection *client, const char *pass, char **response, bool is_manager)
{
    if (!user_exists(client->username) || !pass_valid(client->username, pass))
    {
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Invalid credentials");
        return sizeof(ERR_RESPONSE(" Invalid credentials")) - 1;
    }

    if (user_locked(client->username))
    {
        client->username[0] = 0;

        *response = ERR_RESPONSE(" User mailbox in use");
        return sizeof(ERR_RESPONSE(" User mailbox in use")) - 1;
    }

    if (!set_lock(client->username))
    {
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Failed to lock mailbox");
        return sizeof(ERR_RESPONSE(" Failed to lock mailbox")) - 1;
    }

    if (!set_user_mails(client->username, client))
    {
        remove_lock(client->username);
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Failed to load user mails");
        return sizeof(ERR_RESPONSE(" Failed to load user mails")) - 1;
    }

    client->authenticated = true;

    *response = OK_RESPONSE(" Logged in");
    return sizeof(OK_RESPONSE(" Logged in")) - 1;
}

/**
 * @brief Handles a NOOP command.
 *
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_noop(char **response)
{
    *response = OK_RESPONSE(" Waiting for something to happen...");
    return sizeof(OK_RESPONSE(" Waiting for something to happen...")) - 1;
}

/**
 * @brief Handles a STAT command.
 * 
 * @note The response must be freed by the caller.
 *
 * @param client The client connection.
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_stat(Connection *client, char **response)
{
    size_t size = 0;
    size_t count = 0;

    for (size_t i = 0; i < client->mail_count; i++)
    {
        Mailfile mail = client->mails[i];

        if (!mail.deleted)
        {
            size += mail.size;
            count++;
        }
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu %zu"), count, size);

    *response = strdup(buffer);
    return POP_MIN(len);
}

/**
 * @brief Handles a LIST command with arguments.
 *
 * @note Doesn't validate the message number is in range.
 * @note The response must be freed by the caller.
 *
 * @param client The client connection.
 * @param msg The message number to list (1-indexed).
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_list(Connection *client, size_t msg, char **response)
{
    Mailfile *mail = client->mails + (msg - 1);

    if (!mail->uid[0])
    {
        *response = ERR_RESPONSE(" No such message");
        return sizeof(ERR_RESPONSE(" No such message")) - 1;
    }

    if (mail->deleted)
    {
        *response = ERR_RESPONSE(" Message already deleted");
        return sizeof(ERR_RESPONSE(" Message already deleted")) - 1;
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu %zu"), msg, mail->size);

    *response = strdup(buffer);
    return POP_MIN(len);
}

/**
 * @brief Handles a LIST command without arguments.
 *
 * @note Multi-line response, handles the sends internally.
 *
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @return KEEP_CONNECTION_OPEN always.
 */
static ON_MESSAGE_RESULT handle_list_all(Connection *client, int client_fd)
{
    size_t size = 0;
    size_t count = 0;

    for (size_t i = 0; i < client->mail_count; i++)
    {
        Mailfile mail = client->mails[i];

        if (!mail.deleted)
        {
            size += mail.size;
            count++;
        }
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu messages (%zu octets)"), count, size);

    asend(client_fd, buffer, POP_MIN(len));

    for (size_t j = 0; j < client->mail_count; j++)
    {
        Mailfile mail = client->mails[j];

        if (mail.deleted)
        {
            continue;
        }

        char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
        size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, "%zu %zu" POP3_ENTER, j + 1, mail.size);

        asend(client_fd, buffer, POP_MIN(len));
    }

    asend(client_fd, "." POP3_ENTER, sizeof("." POP3_ENTER) - 1);

    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles the RETR plumbing
 *
 * @note I recommend reading this method from the bottom up
 *
 * @param filename The filename to retrieve.
 * @return int The file descriptor to read the transformed file.
 */
static int handle_retr_plumbing(char *filename)
{
    char *transformer = get_transformer();

    int output_pipes[2];
    if (pipe(output_pipes))
    {
        return -1;
    }

    int pid = fork();
    if (pid < 0)
    {
        return -1;
    }

    if (!pid)
    {
        int transformer_pipes[2];
        if (pipe(transformer_pipes))
        {
            _exit(EXIT_FAILURE);
        }

        int stuffer_pipes[2];
        if (pipe(stuffer_pipes))
        {
            _exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid < 0)
        {
            _exit(EXIT_FAILURE);
        }

        if (!pid)
        {
            close(transformer_pipes[0]);
            close(transformer_pipes[1]);
            close(stuffer_pipes[1]);
            close(output_pipes[0]);

            dup2(stuffer_pipes[0], STDIN_FILENO);
            dup2(output_pipes[1], STDOUT_FILENO);

            execlp(stuffer, stuffer, NULL);
            _exit(EXIT_FAILURE);
        }

        pid = fork();
        if (pid < 0)
        {
            _exit(EXIT_FAILURE);
        }

        if (!pid)
        {
            close(transformer_pipes[1]);
            close(stuffer_pipes[0]);
            close(output_pipes[0]);
            close(output_pipes[1]);

            dup2(transformer_pipes[0], STDIN_FILENO);
            dup2(stuffer_pipes[1], STDOUT_FILENO);

            execlp(transformer, transformer, NULL);
            _exit(EXIT_FAILURE);
        }

        close(transformer_pipes[0]);
        close(stuffer_pipes[0]);
        close(stuffer_pipes[1]);
        close(output_pipes[0]);
        close(output_pipes[1]);

        dup2(transformer_pipes[1], STDOUT_FILENO);

        execlp("cat", "cat", filename, NULL);
        _exit(EXIT_FAILURE);
    }

    close(output_pipes[1]);
    return output_pipes[0];
}

/**
 * @brief Handles a RETR command.
 *
 * @note Multi-line response, handles the sends internally.
 *
 * @param client The client connection.
 * @param msg The message number to retrieve (1-indexed).
 * @param client_fd The client file descriptor.
 * @return KEEP_CONNECTION_OPEN always.
 */
static ON_MESSAGE_RESULT handle_retr(Connection *client, size_t msg, int client_fd)
{
    char *maildir = get_maildir();

    Mailfile *mail = client->mails + (msg - 1);

    if (!mail->uid[0])
    {
        char response[] = ERR_RESPONSE(" No such message");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    if (mail->deleted)
    {
        char response[] = ERR_RESPONSE(" Message already deleted");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/cur/") + sizeof(mail->uid)];
    snprintf(path, sizeof(path), "%s/%s/cur/%s", maildir, client->username, mail->uid);

    if (access(path, F_OK))
    {
        char response[] = ERR_RESPONSE(" Failed to read message");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    int pipe = handle_retr_plumbing(path);
    if (pipe < 0)
    {
        char response[] = ERR_RESPONSE(" Internal error");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    FILE *transformed = fdopen(pipe, "r");
    if (!transformed)
    {
        close(pipe);
        char response[] = ERR_RESPONSE(" Internal error");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    setvbuf(transformed, NULL, _IONBF, 0);

    char buffer[] = OK_RESPONSE();
    asend(client_fd, buffer, sizeof(buffer) - 1);

    fasend(client_fd, transformed, fclose);
    asend(client_fd, POP3_ENTER "." POP3_ENTER, sizeof(POP3_ENTER "." POP3_ENTER) - 1);

    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles a DELE command.
 *
 * @param client The client connection.
 * @param msg The message number to delete (1-indexed).
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_dele(Connection *client, size_t msg, char **response)
{
    Mailfile *mail = client->mails + (msg - 1);

    if (!mail->uid[0])
    {
        *response = ERR_RESPONSE(" No such message");
        return sizeof(ERR_RESPONSE(" No such message")) - 1;
    }

    if (mail->deleted)
    {
        *response = ERR_RESPONSE(" Message already deleted");
        return sizeof(ERR_RESPONSE(" Message already deleted")) - 1;
    }

    mail->deleted = true;

    *response = OK_RESPONSE(" Message deleted");
    return sizeof(OK_RESPONSE(" Message deleted")) - 1;
}

/**
 * @brief Handles a RSET command.
 * 
 * @param client The client connection.
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_rset(Connection *client, char **response)
{
    for (size_t i = 0; i < client->mail_count; i++)
    {
        client->mails[i].deleted = false;
    }

    *response = OK_RESPONSE(" Reversed deletes");
    return sizeof(OK_RESPONSE(" Reversed deletes")) - 1;
}

/**
 * @brief Handles a UIDL command with arguments.
 *
 * @note Doesn't validate the message number is in range.
 * @note The response must be freed by the caller.
 *
 * @param client The client connection.
 * @param msg The message number to retrieve (1-indexed).
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_uidl(Connection *client, size_t msg, char **response)
{
    Mailfile mail = client->mails[msg - 1];

    if (!mail.uid[0])
    {
        *response = ERR_RESPONSE(" No such message");
        return sizeof(ERR_RESPONSE(" No such message")) - 1;
    }

    if (mail.deleted)
    {
        *response = ERR_RESPONSE(" Message already deleted");
        return sizeof(ERR_RESPONSE(" Message already deleted")) - 1;
    }

    char *splitter = strchr(mail.uid, ':');

    if (!splitter || mail.uid == splitter)
    {
        *response = ERR_RESPONSE(" Internal error");
        return sizeof(ERR_RESPONSE(" Internal error")) - 1;
    }

    size_t index = splitter - mail.uid;

    char *uid = strdup(mail.uid);
    uid[index] = 0;

    char *buffer = malloc(MAX_POP3_RESPONSE_LENGTH + 1);
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu %.70s"), msg, uid);

    free(uid);

    *response = buffer;
    return POP_MIN(len);
}

/**
 * @brief Handles a UIDL command with arguments.
 *
 * @note Multi-line response, handles the sends internally.
 *
 * @param client The client connection.
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static ON_MESSAGE_RESULT handle_uidl_all(Connection *client, int client_fd)
{
    asend(client_fd, OK_RESPONSE(), sizeof(OK_RESPONSE()) - 1);

    for (size_t i = 0; i < client->mail_count; i++)
    {
        Mailfile mail = client->mails[i];

        if (mail.deleted)
        {
            continue;
        }

        char *splitter = strchr(mail.uid, ':');

        if (!splitter || mail.uid == splitter)
        {
            LOG("Unexpected filename format: %s", mail.uid);
            continue;
        }

        size_t index = splitter - mail.uid;

        char *uid = strdup(mail.uid);
        uid[index] = 0;

        char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
        size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, "%zu %.70s" POP3_ENTER, i + 1, uid);

        free(uid);

        asend(client_fd, buffer, POP_MIN(len));
    }

    asend(client_fd, "." POP3_ENTER, sizeof("." POP3_ENTER) - 1);

    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles a message in the authorization state of a POP3 connection.
 *
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @param is_manager If the client is a manager.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_pop_authorization_state(Connection *client, int client_fd, char *body, size_t length, bool is_manager)
{
    char cmds[length + 1];
    strncpy(cmds, body, length);
    cmds[length] = 0;

    int argc = parse_pop_cmd(cmds);

    char *buffer;

    if (!strcmp(cmds, "QUIT"))
    {
        // The "bye" message will be sent by the on_close event
        return CLOSE_CONNECTION;
    }

    if (client->username[0])
    {
        if (!strcmp(cmds, "PASS"))
        {
            if (argc < 1)
            {
                char response[] = ERR_RESPONSE(" Invalid number of arguments");
                asend(client_fd, response, sizeof(response) - 1);
                return KEEP_CONNECTION_OPEN;
            }

            // Spaces are accepted as part of the password, so don't use the parsed args
            size_t len = handle_pass(client, body + 5, &buffer, is_manager);
            asend(client_fd, buffer, len);
            return KEEP_CONNECTION_OPEN;
        }

        char response[] = ERR_RESPONSE(" Expected PASS command");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "USER"))
    {
        if (argc != 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        size_t len = handle_user(client, cmds + sizeof("USER"), &buffer);
        asend(client_fd, buffer, len);
        return KEEP_CONNECTION_OPEN;
    }

    char response[] = ERR_RESPONSE(" Invalid command");
    asend(client_fd, response, sizeof(response) - 1);
    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles a message in the transaction state of a POP3 connection.
 *
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_pop_transaction_state(Connection *client, int client_fd, char *body, size_t length)
{
    char cmds[length + 1];
    strncpy(cmds, body, length);
    cmds[length] = 0;

    int argc = parse_pop_cmd(cmds);

    char *buffer;

    if (!strcmp(cmds, "QUIT"))
    {
        client->update = true;
        return CLOSE_CONNECTION;
    }

    if (!strcmp(cmds, "NOOP"))
    {
        size_t len = handle_noop(&buffer);
        asend(client_fd, buffer, len);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "STAT"))
    {
        size_t len = handle_stat(client, &buffer);
        asend(client_fd, buffer, len);
        free(buffer);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "RSET"))
    {
        size_t len = handle_rset(client, &buffer);
        asend(client_fd, buffer, len);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "DELE"))
    {
        if (argc != 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *num = cmds + sizeof("DELE");

        char *err;
        size_t msg = strtoull(num, &err, 10);

        if (*err || !(0 < msg && msg <= client->mail_count) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        size_t len = handle_dele(client, msg, &buffer);
        asend(client_fd, buffer, len);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "LIST"))
    {
        if (argc > 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        if (!argc)
        {
            return handle_list_all(client, client_fd);
        }

        char *num = cmds + sizeof("LIST");

        char *err;
        size_t msg = strtoull(num, &err, 10);

        if (*err || !(0 < msg && msg <= client->mail_count) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        size_t len = handle_list(client, msg, &buffer);
        asend(client_fd, buffer, len);
        free(buffer);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "RETR"))
    {
        if (argc != 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *num = cmds + sizeof("RETR");

        char *err;
        size_t msg = strtoull(num, &err, 10);

        if (*err || !(0 < msg && msg <= client->mail_count) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        return handle_retr(client, msg, client_fd);
    }

    if (!strcmp(cmds, "UIDL"))
    {
        if (argc > 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        if (!argc)
        {
            return handle_uidl_all(client, client_fd);
        }

        char *num = cmds + sizeof("LIST");

        char *err;
        size_t msg = strtoull(num, &err, 10);

        if (*err || !(0 < msg && msg <= client->mail_count) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        size_t len = handle_uidl(client, msg, &buffer);
        asend(client_fd, buffer, len);
        free(buffer);
        return KEEP_CONNECTION_OPEN;
    }

    char response[] = ERR_RESPONSE(" Invalid command");
    asend(client_fd, response, sizeof(response) - 1);
    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles a message in the manager state of a Manager connection.
 * 
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_manager_state(Connection *client, int client_fd, char *body, size_t length)
{
    char cmds[length + 1];
    strncpy(cmds, body, length);
    cmds[length] = 0;

    int argc = parse_pop_cmd(cmds);

    if (!strcmp(cmds, "QUIT"))
    {
        return CLOSE_CONNECTION;
    }

    if (!strcmp(cmds, "GET"))
    {
        if (argc != 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *key = cmds + sizeof("GET");

        if (!strcmp(key, "maildir"))
        {
            char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
            size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %s"), get_maildir());

            asend(client_fd, buffer, len);
            return KEEP_CONNECTION_OPEN;
        }

        if (!strcmp(key, "transformer"))
        {
            char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
            size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %s"), get_transformer());

            asend(client_fd, buffer, len);
            return KEEP_CONNECTION_OPEN;
        }
    }

    if (!strcmp(cmds, "SET"))
    {
        if (argc != 2)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *key = cmds + sizeof("SET");
        char *value = key + strlen(key) + 1;

        if (!strcmp(key, "maildir"))
        {
            set_maildir(value);

            char response[] = OK_RESPONSE(" Maildir set");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        if (!strcmp(key, "transformer"))
        {
            set_transformer(value);

            char response[] = OK_RESPONSE(" Transformer set");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }
    }

    if (!strcmp(cmds, "ADD"))
    {
        if (argc != 2)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *username = cmds + sizeof("ADD");
        char *password = username + strlen(username) + 1;

        bool edited = !!get_user(username);

        if (set_user(username, password))
        {
            char response[] = OK_RESPONSE(" Failed to add user");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        if (edited)
        {
            char response[] = OK_RESPONSE(" User updated");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char response[] = OK_RESPONSE(" User added");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    if (!strcmp(cmds, "DELE"))
    {
        if (argc != 1)
        {
            char response[] = ERR_RESPONSE(" Invalid number of arguments");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char *username = cmds + sizeof("DELE");

        if (delete_user(username))
        {
            char buffer[] = OK_RESPONSE(" User not found");
            asend(client_fd, buffer, sizeof(buffer) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        char buffer[] = OK_RESPONSE(" User deleted");
        asend(client_fd, buffer, sizeof(buffer) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    char response[] = ERR_RESPONSE(" Invalid command");
    asend(client_fd, response, sizeof(response) - 1);
    return KEEP_CONNECTION_OPEN;
}

/**
 * @brief Handles a single POP3 command.
 *
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @param cmd The input command (NULL terminated).
 * @param length The length of the input.
 * @param is_manager If the client is a manager.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_pop_single_cmd(Connection *client, int client_fd, char *cmd, size_t length, bool is_manager)
{
    // If the client is already on the UPDATE state, ignore the message
    if (client->update)
    {
        return KEEP_CONNECTION_OPEN;
    }

    // Authorization state
    if (!client->authenticated)
    {
        return handle_pop_authorization_state(client, client_fd, cmd, length, is_manager);
    }

    if (is_manager)
    {
        return handle_manager_state(client, client_fd, cmd, length);
    }

    return handle_pop_transaction_state(client, client_fd, cmd, length);
}

ON_MESSAGE_RESULT handle_pop_connect(int client_fd, struct sockaddr_in address, const short port)
{
    const short manager_port = ntohs(get_manager_adport().sin_port);

    if (port == manager_port && active_managers >= MAX_ADMIN_CONNECTIONS)
    {
        return CONNECTION_ERROR;
    }

    connections[client_fd] = calloc(1, sizeof(Connection));

    if (!connections[client_fd])
    {
        return CONNECTION_ERROR;
    }

    if (port == manager_port)
    {
        active_managers++;
        char response[] = OK_RESPONSE(" Manager ready");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    char response[] = OK_RESPONSE(" POP3 server ready");
    asend(client_fd, response, sizeof(response) - 1);
    return KEEP_CONNECTION_OPEN;
}

ON_MESSAGE_RESULT handle_pop_message(int client_fd, const char *body, size_t length, const short port)
{
    bool is_manager = port == ntohs(get_manager_adport().sin_port);

    Connection *client = connections[client_fd];

    char buffer[length];
    strncpy(buffer, body, length);

    char *start_cmd = buffer;
    for (size_t i = 0; i < length; i++)
    {
        if (buffer + i != start_cmd && buffer[i - 1] == '\r' && buffer[i] == '\n')
        {
            buffer[i - 1] = 0;

            char *data = start_cmd;
            if (client->buffer[0])
            {
                // If the buffer is full, drop the connection
                if (sizeof(client->buffer) - strlen(client->buffer) - 1 < length - (start_cmd - buffer))
                {
                    return CONNECTION_ERROR;
                }

                strncat(client->buffer, start_cmd, sizeof(client->buffer) - strlen(client->buffer) - 1);
                data = client->buffer;
            }

            ON_MESSAGE_RESULT result = handle_pop_single_cmd(client, client_fd, data, strlen(data), is_manager);

            client->buffer[0] = 0;

            if (result != KEEP_CONNECTION_OPEN)
            {
                return result;
            }

            start_cmd = buffer + i + 1;
        }
    }

    // If there is a command left in the body, store it for the next message
    if (start_cmd != buffer + length)
    {
        size_t before_len = strlen(client->buffer);
        size_t remaining_len = length - (start_cmd - buffer);

        // If the buffer is full, drop the connection
        if (sizeof(client->buffer) - before_len - 1 < remaining_len)
        {
            return CONNECTION_ERROR;
        }

        strncpy(client->buffer + before_len, start_cmd, remaining_len);
        client->buffer[before_len + remaining_len] = 0;
    }
    else
    {
        client->buffer[0] = 0;
    }

    return KEEP_CONNECTION_OPEN;
}

void handle_pop_close(int client_fd, ON_MESSAGE_RESULT result, const short port)
{
    Connection *client = connections[client_fd];

    if (port == ntohs(get_manager_adport().sin_port))
    {
        active_managers--;
        goto COMMON_CONNECTIONS_CLOSE;
    }

    if (result == CONNECTION_ERROR)
    {
        client->update = false;
    }

    if (client->update)
    {
        char *maildir = get_maildir();

        for (size_t i = 0; i < client->mail_count; i++)
        {
            Mailfile mail = client->mails[i];

            if (!mail.deleted)
            {
                continue;
            }

            char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/cur/") + sizeof(mail.uid)];
            snprintf(path, sizeof(path), "%s/%s/cur/%s", maildir, client->username, mail.uid);

            if (remove(path) < 0)
            {
                LOG("Failed to remove mail %s\n", mail.uid);
            }
        }
    }

    if (client->authenticated && !remove_lock(client->username))
    {
        LOG("Failed to remove lock for %s\n", client->username);
    }

    if (client->mails)
    {
        free(client->mails);
    }

COMMON_CONNECTIONS_CLOSE:
    // Privacy friendly
    memset(client->username, 0, sizeof(client->username));

    free(client);

    if (result != CONNECTION_ERROR)
    {
        asend(client_fd, OK_RESPONSE(" Bye!"), sizeof(OK_RESPONSE(" Bye!")) - 1);
    }
}

#include <pop.h>

#include <ctype.h>
#include <dirent.h>
#include <logger.h>
#include <magic.h>
#include <math.h>
#include <pthread.h>
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

/**
 * @brief The mail information for a client connection.
 */
typedef struct Mailfile
{
    /**
     * @brief The mail unique ID and filename
     */
    char uid[71];
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
    Mailfile mails[MAX_CLIENT_MAILS];
} Connection;

static Connection connections[MAGIC_NUMBER] = {0};

/**
 * @brief The directory where the users mailboxes are stored.
 */
static const char *maildir;
/**
 * @brief The transformer to use for the mail messages.
 */
static const char *mutator;
/**
 * @brief The path to the bytestuffer program.
 */
static const char *stuffer;

void pop_init(const char *dir, const char *transformer, const char *bytestuffer)
{
    maildir = dir ? dir : "./dist/mail";
    mutator = transformer ? transformer : "cat";
    stuffer = bytestuffer ? bytestuffer : "./dist/bytestuff";

    if (access(maildir, F_OK) == -1)
    {
        mkdir(maildir, S_IRWXU);
    }

    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        connections[i].username[0] = 0;
        connections[i].authenticated = false;
        connections[i].update = false;
    }
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
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + 1];
    snprintf(path, sizeof(path), "%s/%s", maildir, username);
    return access(path, F_OK) != -1;
}

/**
 * @brief Validate if a user mail directory isn't locked.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The user directory is locked.
 * @return false The user directory is not locked.
 */
static bool user_locked(const char *username)
{
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);
    return access(path, F_OK) != -1;
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
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);

    FILE *file = fopen(path, "w");
    if (!file)
    {
        return false;
    }

    fclose(file);
    return true;
}

/**
 * @brief Remove a maildir lock.
 * @note The username MUST be a safe string. Use safe_username() to validate.
 *
 * @param username The input username (NULL terminated).
 * @return true The lock was successfully removed.
 * @return false The lock could not be removed (or did not exist).
 */
static bool remove_lock(const char *username)
{
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);

    return remove(path) == 0;
}

/**
 * @brief Validate a user password.
 * @note Does NOT validate the username is secure.
 *
 * @param username The username (NULL terminated).
 * @param pass The input password (NULL terminated).
 * @return true The password is correct.
 * @return false The password is incorrect (or failed to read the password file).
 */
static bool pass_valid(const char *username, const char *pass)
{
    char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/data/pass")];
    snprintf(path, sizeof(path), "%s/%s/data/pass", maildir, username);

    FILE *file = fopen(path, "r");
    if (!file)
    {
        return false;
    }

    char buffer[MAX_PASSWORD_LENGTH + 1];
    char *success = fgets(buffer, sizeof(buffer), file);
    fclose(file);

    if (!success)
    {
        return false;
    }

    return !strcmp(buffer, pass);
}

/**
 * @brief Set the user emails in the client connection.
 *
 * @param username The username (NULL terminated).
 * @param mails The client mails array.
 * @return true The mails were successfully loaded.
 * @return false The mails could not be loaded.
 */
static bool set_user_mails(const char *username, Mailfile mails[MAX_CLIENT_MAILS])
{
    char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/mail")];
    snprintf(path, sizeof(path), "%s/%s/mail", maildir, username);

    DIR *dir = opendir(path);
    if (!dir)
    {
        return false;
    }

    size_t i = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        {
            continue;
        }

// This is the expected behaviour, if the filename is too long, truncate it
#pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(mails[i].uid, entry->d_name, sizeof(mails[i].uid) - 1);
        mails[i].uid[sizeof(mails[i].uid) - 1] = 0;
#pragma GCC diagnostic warning "-Wstringop-truncation"

        mails[i].deleted = false;

        char filepath[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/mail/") + sizeof(mails[i].uid)];
        snprintf(filepath, sizeof(filepath), "%s/%s/mail/%s", maildir, username, mails[i].uid);

        struct stat buf;
        if (stat(filepath, &buf) < 0)
        {
            closedir(dir);
            return false;
        }

        mails[i].size = buf.st_size;

        i++;
    }

    mails[i].uid[0] = 0;

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
    if (!safe_username(username) || !user_exists(username))
    {
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Never heard that name before");
        return sizeof(ERR_RESPONSE(" Never heard that name before"));
    }

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
 * @return size_t The length of the response.
 */
static size_t handle_pass(Connection *client, const char *pass, char **response)
{
    if (!pass_valid(client->username, pass))
    {
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Invalid password");
        return sizeof(ERR_RESPONSE(" Invalid password")) - 1;
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

    if (!set_user_mails(client->username, client->mails))
    {
        remove_lock(client->username);
        client->username[0] = 0;

        *response = ERR_RESPONSE(" Failed to load user mails");
        return sizeof(ERR_RESPONSE(" Failed to load user mails")) - 1;
    }

    client->authenticated = true;

    *response = OK_RESPONSE(" Something about the weight of the emails here");
    return sizeof(OK_RESPONSE(" Something about the weight of the emails here")) - 1;
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
 * @param client The client connection.
 * @param response The response to send back to the client.
 * @return size_t The length of the response.
 */
static size_t handle_stat(Connection *client, char **response)
{
    size_t size = 0;
    size_t count = 0;

    Mailfile *mails = client->mails;
    while (mails->uid[0])
    {
        if (!mails->deleted)
        {
            size += mails->size;
            count++;
        }

        mails++;
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu %zu"), count, size);

    *response = buffer;
    return POP_MIN(len);
}

/**
 * @brief Handles a LIST command with arguments.
 *
 * @note Doesn't validate the message number is in range.
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

    *response = buffer;
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

    Mailfile *mails = client->mails;
    while (mails->uid[0])
    {
        if (!mails->deleted)
        {
            size += mails->size;
            count++;
        }

        mails++;
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
    size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, OK_RESPONSE(" %zu messages (%zu octets)"), count, size);

    asend(client_fd, buffer, POP_MIN(len));

    size_t i = 1;
    mails = client->mails;
    while (mails->uid[0])
    {
        if (mails->deleted)
        {
            mails++;
            continue;
        }

        char buffer[MAX_POP3_RESPONSE_LENGTH + 1];
        size_t len = snprintf(buffer, MAX_POP3_RESPONSE_LENGTH, "%zu %zu" POP3_ENTER, i++, mails->size);

        asend(client_fd, buffer, POP_MIN(len));

        mails++;
    }

    asend(client_fd, "." POP3_ENTER, sizeof("." POP3_ENTER) - 1);

    return KEEP_CONNECTION_OPEN;
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
    Mailfile *mail = client->mails + (msg - 1);

    if (!mail->uid[0])
    {
        char response[] = ERR_RESPONSE(" No such message");
        asend(client_fd, response, sizeof(response) - 1);
    }

    if (mail->deleted)
    {
        char response[] = ERR_RESPONSE(" Message already deleted");
        asend(client_fd, response, sizeof(response) - 1);
    }

    char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/mail/") + sizeof(mail->uid)];
    snprintf(path, sizeof(path), "%s/%s/mail/%s", maildir, client->username, mail->uid);

    char cmd[sizeof("cat ") + strlen(path) + sizeof(" | ") + strlen(mutator) + sizeof(" | ") + strlen(stuffer)];
    snprintf(cmd, sizeof(cmd) - 1, "cat %s | %s | %s", path, mutator, stuffer);

    FILE *transformed = popen(cmd, "r");
    setvbuf(transformed, NULL, _IONBF, 0);

    if (!transformed)
    {
        char response[] = ERR_RESPONSE(" Failed to read and transform message");
        asend(client_fd, response, sizeof(response) - 1);
        return KEEP_CONNECTION_OPEN;
    }

    char buffer[] = OK_RESPONSE();
    asend(client_fd, buffer, sizeof(buffer) - 1);

    fasend(client_fd, transformed, pclose);
    asend(client_fd, "." POP3_ENTER, sizeof("." POP3_ENTER) - 1);

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
    Mailfile *mails = client->mails;

    while (mails->uid[0])
    {
        mails->deleted = false;
        mails++;
    }

    *response = OK_RESPONSE(" Reversed deletes");
    return sizeof(OK_RESPONSE(" Reversed deletes")) - 1;
}

/**
 * @brief Handles a message in the authorization state of a POP3 connection.
 *
 * @param client The client connection.
 * @param client_fd The client file descriptor.
 * @param body The message body.
 * @param length The message length.
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_pop_authorization_state(Connection *client, int client_fd, char *body, size_t length)
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
            size_t len = handle_pass(client, body + 5, &buffer);
            asend(client_fd, buffer, len);
            return KEEP_CONNECTION_OPEN;
        }

        char response[] = ERR_RESPONSE(" Expected PASS command");
        asend(client_fd, response, sizeof(response));
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
    asend(client_fd, response, sizeof(response));
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

        if (*err || !(0 < msg && msg < MAX_CLIENT_MAILS) || !isdigit(*num))
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

        if (*err || !(0 < msg && msg < MAX_CLIENT_MAILS) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        size_t len = handle_list(client, msg, &buffer);
        asend(client_fd, buffer, len);
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

        if (*err || !(0 < msg && msg < MAX_CLIENT_MAILS) || !isdigit(*num))
        {
            char response[] = ERR_RESPONSE(" Invalid message number");
            asend(client_fd, response, sizeof(response) - 1);
            return KEEP_CONNECTION_OPEN;
        }

        return handle_retr(client, msg, client_fd);
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
 * @return ON_MESSAGE_RESULT The result of the message handling.
 */
static ON_MESSAGE_RESULT handle_pop_single_cmd(Connection *client, int client_fd, char *cmd, size_t length)
{
    // If the client is already on the UPDATE state, ignore the message
    if (client->update)
    {
        return KEEP_CONNECTION_OPEN;
    }

    // Authorization state
    if (!client->authenticated)
    {
        return handle_pop_authorization_state(client, client_fd, cmd, length);
    }

    // Consider adding an UPDATE flag on the client connection to prevent
    // weird edge case scenarios where a user sends a command after a QUIT
    // command and somehow it triggers an impossible race condition
    return handle_pop_transaction_state(client, client_fd, cmd, length);
}

ON_MESSAGE_RESULT handle_pop_connect(int client_fd, struct sockaddr_in address)
{
    connections[client_fd].buffer[0] = 0;
    connections[client_fd].username[0] = 0;
    connections[client_fd].authenticated = false;
    connections[client_fd].update = false;

    char response[] = OK_RESPONSE(" POP3 server ready");
    asend(client_fd, response, sizeof(response) - 1);
    return KEEP_CONNECTION_OPEN;
}

ON_MESSAGE_RESULT handle_pop_message(int client_fd, const char *body, size_t length)
{
    Connection *client = connections + client_fd;

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
                data = client->buffer;
                strncat(data, start_cmd, sizeof(client->buffer) - strlen(client->buffer) - 1);
            }

            ON_MESSAGE_RESULT result = handle_pop_single_cmd(client, client_fd, data, strlen(data));

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
        strncpy(client->buffer, start_cmd, length - (start_cmd - buffer));
        client->buffer[length - (start_cmd - buffer)] = 0;
    }
    else
    {
        client->buffer[0] = 0;
    }

    return KEEP_CONNECTION_OPEN;
}

void handle_pop_close(int client_fd, ON_MESSAGE_RESULT result)
{
    Connection *client = connections + client_fd;

    if (result == CONNECTION_ERROR)
    {
        return;
    }

    if (client->update)
    {
        Mailfile *mails = client->mails;
        while (mails->uid[0])
        {
            if (!mails->deleted)
            {
                mails++;
                continue;
            }

            char path[strlen(maildir) + sizeof("/") + MAX_USERNAME_LENGTH + sizeof("/mail/") + sizeof(mails->uid)];
            snprintf(path, sizeof(path), "%s/%s/mail/%s", maildir, client->username, mails->uid);

            if (remove(path) < 0)
            {
                LOG("Failed to remove mail %s\n", mails->uid);
            }

            mails++;
        }
    }

    remove_lock(client->username);

    // Privacy friendly
    memset(client->username, 0, sizeof(client->username));
    client->authenticated = false;
    client->update = false;

    asend(client_fd, OK_RESPONSE(" Bye!"), sizeof(OK_RESPONSE(" Bye!")) - 1);
}

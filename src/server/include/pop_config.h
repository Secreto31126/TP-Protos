#ifndef POP_CONFIG_H
#define POP_CONFIG_H

#include <common_config.h>
#include <pop.h>
#include <netutils.h>

#define POP_DEFAULT_PORT 28160 // htons(110)

typedef struct {
    char username[MAX_USERNAME_LENGTH + 1];
    char password[MAX_PASSWORD_LENGTH + 1];
    bool locked;
} User;


char *get_maildir();
char *get_version();
struct sockaddr_in6 get_pop_adport();
char *get_transformer();
size_t get_users_arr(const User **users);
User *get_user(const char *username);

char set_pop_address(const char *new_addr);
char set_pop_port(const char *new_port);
void set_maildir(const char *new_maildir);
void set_transformer(const char *transformer);
char set_user(const char *username, const char *password);
char set_user_lock(const char *username);
char unset_user_lock(const char *username);
char delete_user(const char *username);

void shutdown_pop_configs();
#endif

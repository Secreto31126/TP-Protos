#ifndef POP_CONFIG
#define POP_CONFIG
#include <pop.h>
#include <netutils.h>

#define VERSION 1

typedef struct {
    char username[MAX_USERNAME_LENGTH + 1];
    char password[MAX_PASSWORD_LENGTH];
    bool locked;
} User;


char *get_maildir();
char *get_version();
struct sockaddr_in get_pop_adport();
struct sockaddr_in get_manager_adport();
char *get_transformer();
User *get_users_arr();
User *get_user(const char *username);


char set_pop_address(const char *new_addr);
char set_management_address(const char *new_addr);
char set_pop_port(const char *new_port);
char set_management_port(const char *new_port);
void set_maildir(const char *new_maildir);
void set_transformer(const char *transformer);
char set_user(const char *username, const char *password);
char set_user_lock(const char *username, bool locked);
char delete_user(const char *username);

void shutdown_pop_configs();
#endif
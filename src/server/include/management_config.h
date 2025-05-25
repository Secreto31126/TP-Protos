#ifndef MANAGEMENT_CONFIG_H
#define MANAGEMENT_CONFIG_H

#include <common_config.h>
#include <netutils.h>
#include <pop.h>

#define MANAGER_DEFAULT_PORT 57616 // htons(4321) because why not
#define MAX_ADMINS 4
#define VERSION "1"

typedef struct {
    char username[MAX_USERNAME_LENGTH + 1];
    char password[MAX_PASSWORD_LENGTH + 1];
} Admin;

char *get_version();

struct sockaddr_in6 get_manager_adport();
char set_management_address(const char *new_addr);
char set_management_port(const char *new_port);

Admin *get_admin(const char *username);
char add_admin(const char *username, const char *password);

#endif

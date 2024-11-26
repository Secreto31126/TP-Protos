#ifndef COMMON_CONFIG
#define COMMON_CONFIG
#include <pop.h>

/**
 * @brief Assert safe username string.
 * @note A safe username does not start with a dot, does not contain slashes,
 * is not empty and is shorter than MAX_USERNAME_LENGTH characters.
 *
 * @param username The input username (NULL terminated).
 * @return true The username is safe.
 * @return false The username is not safe.
 */
bool safe_username(const char *username);

char set_address(const char *input, struct sockaddr_in6 *address);

char set_port(const char *input, struct sockaddr_in6 *address);

#endif

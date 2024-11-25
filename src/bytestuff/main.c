#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Parse a file and make each line POP3 compliant
 */
int main()
{
    char input[512];
    char prev = '\n';
    while (fgets(input, sizeof(input), stdin))
    {
        size_t len = strlen(input);

        // Just in case something weird happens
        if (!len)
        {
            break;
        }

        char actual = input[len - 1];

        if (prev == '\r' && input[0] == '\n')
        {
            putchar('\n');
            prev = actual;
            continue;
        }

        if (prev == '\n' && input[0] == '.')
        {
            putchar('.');
        }

        if (actual != '\n')
        {
            printf("%s", input);
            prev = actual;
            continue;
        }

        input[len - 1] = '\0';
        if (len != 1 && input[len - 2] == '\r')
        {
            input[len - 2] = '\0';
        }

        printf("%s\r\n", input);

        // Only flush when we are sure we have a full line
        fflush(stdout);
        prev = actual;
    }

    return 0;
}

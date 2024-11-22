#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Parse a file and make each line POP3 compliant
 */
int main()
{
    char buffer[512 + 1];
    while (fgets(buffer, sizeof(buffer), stdin))
    {
        const char *fmt = "%.510s\r\n";

        size_t l = strlen(buffer);

        bool starts_with_dot = buffer[0] == '.';
        size_t max_length = starts_with_dot ? 509 : 510;
        bool ends_with_crlf = l > 2 ? buffer[l - 2] == '\r' && buffer[l - 1] == '\n' : false;

        if (starts_with_dot)
        {
            // Prepend a .
            fmt = ".%.509s\r\n";
        }

        if (ends_with_crlf)
        {
            buffer[l - 2] = '\0';
        }
        else if (l < max_length && buffer[l - 1] == '\n')
        {
            buffer[l - 1] = '\0';
        }
        // else assume the buffer doesn't end with any form of enter

        printf(fmt, buffer);
    }

    return 0;
}

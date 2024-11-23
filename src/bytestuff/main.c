#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BLOCK 512

/**
 * @brief Parse a file and make each line POP3 compliant
 */
int main()
{
    char s[BLOCK];
    int read_is_after_newline = 1, read_is_after_carriage_return = 0, last_print, i;
    while (fgets(s, BLOCK, stdin) != NULL)
    {
        last_print = 0;
        if (s[0] == '\n' && !read_is_after_carriage_return)
            putchar('\r');
        else if (s[0] == '.' && read_is_after_newline)
            putchar('.');
        for (i = 1; s[i]; i++)
        {
            if (s[i] == '\n' && s[i - 1] != '\r')
            {
                fwrite(s + last_print, sizeof(char), i - last_print, stdout);
                putchar('\r');
                last_print = i;
            }
            else if (s[i] == '.' && s[i - 1] == '\n')
            {
                fwrite(s + last_print, sizeof(char), i - last_print + 1, stdout);
                putchar('.');
                last_print = i;
            }
        }
        if (i > last_print)
            fwrite(s + last_print, sizeof(char), i - last_print, stdout);
        if (i > 0)
        {
            if (s[i - 1] == '\n')
                read_is_after_newline = 1;
            else
            {
                read_is_after_newline = 0;
                if (s[i - 1] == '\r')
                    read_is_after_carriage_return = 1;
                else
                    read_is_after_carriage_return = 0;
            }
        }
    }
    return 0;
}

#include <argument_parser.h>
#include <management_config.h>
#include <pop_config.h>

static void usage();
static void help();

static const char *_progname;

void parse_arguments(int argc, const char *argv[], const char *progname)
{
    _progname = progname;
    int added_users = 0;
    int added_admins = 0;
    for (size_t i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
            case 'h':
                help();
                exit(0);
                break;
            case 'v':
                printf("Version: 1\n");
                exit(0);
                break;
            case 'd':
                set_maildir(argv[++i]);
                break;
            case 'l':
                set_pop_address(argv[++i]);
                break;
            case 'L':
                set_management_address(argv[++i]);
                break;
            case 'p':
                set_pop_port(argv[++i]);
                break;
            case 'P':
                set_management_port(argv[++i]);
                break;
            case 't':
                set_transformer(argv[++i]);
                break;
            case 'u':
                while(++i < argc && argv[i][0] != '-')
                {
                    if(added_users == 10)
                    {
                        printf("Max users reached\n");
                        exit(1);
                    }
                    char *user = strdup(argv[i]);
                    char *pass = strchr(user, ':');
                    if(pass == NULL)
                    {
                        printf("Pass must be between 1 an 40 chars long\n");
                        exit(1);
                    }
                    *pass = '\0';
                    pass++;
                    if(set_user(user, pass) != 0)
                    {
                        printf("User must be between 1 an 40 chars long\n");
                        printf("Pass must be between 1 an 40 chars long\n");
                        exit(1);
                    }
                    free(user);
                    added_users++;
                }
                i--;
                break;
            case 'a':
                while(++i < argc && argv[i][0] != '-')
                {
                    if(added_admins == 4)
                    {
                        printf("Max admins reached\n");
                        exit(1);
                    }
                    char *admin = strdup(argv[i]);
                    char *pass = strchr(admin, ':');
                    if(pass == NULL)
                    {
                        printf("Pass must be between 1 an 40 chars long\n");
                        exit(1);
                    }
                    *pass = '\0';
                    pass++;
                    if(add_admin(admin, pass) != 0)
                    {
                        printf("User must be between 1 an 40 chars long\n");
                        printf("Pass must be between 1 an 40 chars long\n");
                        exit(1);
                    }
                    free(admin);
                    added_admins++;
                }
                i--;
                break;
            default:
                usage();
                break;
            }
        }
    }
}

static void printUsage(FILE *fd)
{
    fprintf(fd,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <POP3 addr>   Dirección donde servirá el servidor POP.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
            "   -a <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor de administración. Hasta 4.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"
            "   -d <dir>         Carpeta donde residen los Maildirs\n"
            "   -t <cmd>         Comando para aplicar transformaciones\n"
            "\n",
            _progname);
}

static void usage()
{
    printUsage(stderr);
    exit(1);
}

static void help()
{
    printUsage(stdout);
}

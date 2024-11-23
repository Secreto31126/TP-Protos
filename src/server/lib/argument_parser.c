#include <argument_parser.h>
#include <pop_config.h>

static void usage();
static void help();

static const char *_progname;

void parse_arguments(int argc, const char *argv[], const char *progname, struct sockaddr_in *address_pop, struct sockaddr_in *address_conf, const char **dir_path)
{
    _progname = progname;
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
                //TODO Loop para agregar todos los usuarios
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
            "   -v               Imprime información sobre la versión versión y termina.\n"
            "   -d <dir>         Carpeta donde residen los Maildirs\n"
            "   -t <cmd>         Comando para aplicar transformaciones (WIP)\n"
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

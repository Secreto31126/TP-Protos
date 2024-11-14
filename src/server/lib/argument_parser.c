#include <argument_parser.h>

static void usage();
static void help();
static void set_address(const char *input, struct sockaddr_in *address);
static void set_port(const char *input, struct sockaddr_in *address);
static void set_maildir(const char **dir_path, const char *arg);

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
                help("server");
                exit(0);
                break;
            case 'v':
                printf("Version: 1\n");
                exit(0);
                break;
            case 'd':
                set_maildir(dir_path, argv[++i]);
                break;
            case 'l':
                set_address(argv[++i], address_pop);
                break;
            case 'L':
                set_address(argv[++i], address_conf);
                break;
            case 'p':
                set_port(argv[++i], address_pop);
                break;
            case 'P':
                set_port(argv[++i], address_conf);
                break;
            default:
                usage();
                break;
            }
        }
    }
}

static void set_address(const char *input, struct sockaddr_in *address)
{
    int aux = inet_pton(AF_INET, input, &(address->sin_addr.s_addr));
    if (aux < 0)
    {
        aux = inet_pton(AF_INET6, input, &(address->sin_addr.s_addr));
        if (aux < 0)
        {
            fprintf(stderr, "Addr argument should be a valid IP address (In case of IPv4: 'ddd.ddd.ddd.ddd', where ddd is a"
                            " decimal number of up to three digits in the range 0 to 255."
                            " In case of IPv6: see RFC 2373 for details on the repeesentation of IPv6 addresses)."
                            "\nRun server with argument '-h' to see usage.\n");
            exit(1);
        }
        address->sin_family = AF_INET6;
    }
}

static void set_port(const char *input, struct sockaddr_in *address)
{
    __u_long port = strtol(input, NULL, 10);
    if (port <= 0 || port > USHRT_MAX)
    {
        fprintf(stderr, "Port argument should be a number between 1 and 65535. Run server with argument '-h' to see usage.\n");
        exit(1);
    }
    address->sin_port = htons((__u_short)port);
}

static void set_maildir(const char **dir_path, const char *arg)
{
    *dir_path = arg;
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

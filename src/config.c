/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / This is configuration module, here we parse configuration   \
        | passed by the user. Configuration can be accessed by global |
        | g_config variable that is not modified outside this module. |
        \ Never.                                                      /
         -------------------------------------------------------------
            \                          ___-------___
             \                     _-~~             ~~-_
              \                 _-~                    /~-_
             /^\__/^\         /~  \                   /    \
           /|  O|| O|        /      \_______________/        \
          | |___||__|      /       /                \          \
          |          \    /      /                    \          \
          |   (_______) /______/                        \_________ \
          |         / /         \                      /            \
           \         \^\\         \                  /               \     /
             \         ||           \______________/      _-_       //\__//
               \       ||------_-~~-_ ------------- \ --/~   ~\    || __/
                 ~-----||====/~     |==================|       |/~~~~~
                  (_(__/  ./     /                    \_\      \.
                         (_(___/                         \_____)_)
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#ifdef HAVE_CONFIG_H
#   include "kurload.h"
#endif

#include "feature.h"

#include "config.h"
#include "getopt.h"
#include "globals.h"
#include "valid.h"

#include <ctype.h>
#include <embedlog.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */

/* list of short options for getopt_long */

static const char  *shortopts =
    ":hvcDl:i:a:s:m:t:T:b:d:u:g:q:p:P:o:L:M:"
#if HAVE_SSL
    "I:A:k:C:f:"
#endif
    ;

/* array of long options for getopt_long */

struct option       longopts[] =
{
    {"help",                  no_argument,       NULL, 'h'},
    {"version",               no_argument,       NULL, 'v'},
    {"level",                 required_argument, NULL, 'l'},
    {"colorful-output",       no_argument,       NULL, 'c'},
    {"listen-port",           required_argument, NULL, 'i'},
    {"timed-listen-port",     required_argument, NULL, 'a'},
    {"max-filesize",          required_argument, NULL, 's'},
    {"daemonize",             no_argument,       NULL, 'D'},
    {"max-connections",       required_argument, NULL, 'm'},
    {"max-timeout",           required_argument, NULL, 't'},
    {"timed-max-timeout",     required_argument, NULL, 'M'},
    {"list-type",             required_argument, NULL, 'T'},
    {"bind-ip",               required_argument, NULL, 'b'},
    {"domain",                required_argument, NULL, 'd'},
    {"user",                  required_argument, NULL, 'u'},
    {"group",                 required_argument, NULL, 'g'},
    {"query-log",             required_argument, NULL, 'q'},
    {"program-log",           required_argument, NULL, 'p'},
    {"pid-file",              required_argument, NULL, 'P'},
    {"output-dir",            required_argument, NULL, 'o'},
    {"list-file",             required_argument, NULL, 'L'},
#if HAVE_SSL
    {"ssl-listen-port",       required_argument, NULL, 'I'},
    {"timed-ssl-listen-port", required_argument, NULL, 'A'},
    {"key-file",              required_argument, NULL, 'k'},
    {"cert-file",             required_argument, NULL, 'C'},
    {"pem-pass-file",         required_argument, NULL, 'f'},
#endif
    {NULL, 0, NULL, 0}
};


/* ==========================================================================
                                   _                __
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
               ____                     __   _
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


/* ==========================================================================
    parses arguments passed from command line and overwrites whatever has
    been set in configuration file
   ========================================================================== */


static int config_parse_arguments
(
    int    argc,        /* number of arguments in argv */
    char  *argv[]       /* argument list */
)
{
    /* macros to parse arguments in switch(opt) block */

    /* check if optarg is between MINV and MAXV values and if so,
     * store converted optarg in to config.OPTNAME field. If error
     * occurs, force function to return with -1 error
     */


#   define PARSE_INT(OPTNAME, MINV, MAXV)                                      \
    {                                                                          \
        long   val;     /* value converted from OPTARG */                      \
        char  *endptr;  /* pointer for errors fron strtol */                   \
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/   \
                                                                               \
        val = strtol(optarg, &endptr, 10);                                     \
                                                                               \
        if (*endptr != '\0')                                                   \
        {                                                                      \
            /* error occured */                                                \
                                                                               \
            fprintf(stderr, "wrong value '%s' for option '%s\n",               \
                optarg, #OPTNAME);                                             \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        if (val < (long)MINV || (long)MAXV < val)                              \
        {                                                                      \
            /* number is outside of defined domain */                          \
                                                                               \
            fprintf(stderr, "value for '%s' should be between %ld and %ld\n",  \
                #OPTNAME, (long)MINV, (long)MAXV);                             \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        g_config.OPTNAME = val;                                                \
    }


    /* check if string length of optarg is less or equal than
     * sizeof(config.OPTNAME) and if so store optarg in
     * config.OPTNAME.  If error occurs, force  function  to return
     * with -1 error.
     */


#   define PARSE_STR(OPTNAME)                                                  \
    {                                                                          \
        if (strlen(optarg) >= sizeof(g_config.OPTNAME))                        \
        {                                                                      \
            fprintf(stderr, "value '%s' for '%s' is too long, max is %zu\n",   \
                optarg, #OPTNAME, sizeof(g_config.OPTNAME) - 1);               \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        strcpy(g_config.OPTNAME, optarg);                                      \
    }


    int  arg;
    int  loptind;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    optind = 1;
    while ((arg = getopt_long(argc, argv, shortopts, longopts, &loptind)) != -1)
    {
        switch (arg)
        {
        case 'c': g_config.colorful_output = 1; break;
        case 'D': g_config.daemonize = 1; break;
        case 'l': PARSE_INT(log_level, 0, 7); break;
        case 'i': PARSE_INT(listen_port, 0, UINT16_MAX); break;
        case 'a': PARSE_INT(timed_listen_port, 0, UINT16_MAX); break;
        case 's': PARSE_INT(max_size, 0, LONG_MAX); break;
        case 'm': PARSE_INT(max_connections, 0, LONG_MAX); break;
        case 't': PARSE_INT(max_timeout, 1, LONG_MAX); break;
        case 'M': PARSE_INT(timed_max_timeout, 1, LONG_MAX); break;
        case 'T': PARSE_INT(list_type, -1, 1); break;
        case 'b': PARSE_STR(bind_ip); break;
        case 'd': PARSE_STR(domain); break;
        case 'u': PARSE_STR(user); break;
        case 'g': PARSE_STR(group); break;
        case 'q': PARSE_STR(query_log); break;
        case 'p': PARSE_STR(program_log); break;
        case 'P': PARSE_STR(pid_file); break;
        case 'o': PARSE_STR(output_dir); break;
        case 'L': PARSE_STR(list_file); break;
#if HAVE_SSL
        case 'I': PARSE_INT(ssl_listen_port, 0, UINT16_MAX); break;
        case 'A': PARSE_INT(timed_ssl_listen_port, 0, UINT16_MAX); break;
        case 'k': PARSE_STR(key_file); break;
        case 'C': PARSE_STR(cert_file); break;
        case 'f': PARSE_STR(pem_pass_file); break;
#endif

        case 'h':
            printf(
"kurload - easy file sharing\n"
"\n"
"Usage: %s [-h | -v | options]\n"
"\n"
"options:\n"
"\t-h, --help                       prints this help and quits\n"
"\t-v, --version                    prints version and quits\n"
"\t-l, --level=<level>              logging level 0-7\n"
"\t-c, --colorful-output            enable nice colors for logs\n"
"\t-i, --listen-port=<port>         port on which program will listen\n"
"\t-a, --timed-listen-port=<port>   port on which program will listen\n"
#if HAVE_SSL
"\t-I, --ssl-listen-port=<port>     ssl port on which program will listen\n"
"\t-A, --timed-ssl-listen-port=<port>  ssl port on which program will listen\n"
"\t-k, --key-file=<path>            path to ssl key file\n"
"\t-C, --cert-file=<path>           path to ssl cert file\n"
"\t-f, --pem-pass-file=<path>       path where password for key is stored\n"
#endif
,argv[0]);
            printf(
"\t-s, --max-filesize=<size>        maximum size of file client can upload\n"
"\t-D, --daemonize                  run as daemon\n"
"\t-m, --max-connections=<number>   max number of concurrent connections\n"
"\t-t, --max-timeout=<seconds>      time before client is presumed dead\n"
"\t-M, --timed-max-timeout=<seconds>  inactivity time before accepting data\n"
"\t-T, --list-type=<type>           type of the list_file (black or white)\n"
"\t-L, --list_file=<path>           path with ip list for black/white list\n"
"\t-b, --bind-ip=<ip-list>          comma separated list of ips to bind to\n");
            printf(
"\t-d, --domain=<domain>            domain on which server works\n"
"\t-u, --user=<user>                user that should run daemon\n"
"\t-g, --group=<group>              group that should run daemon\n");
            printf(
"\t-q, --query-log=<path>           where to store query logs\n"
"\t-p, --program-log=<path>         where to store program logs\n"
"\t-P, --pid-file=<path>            where to store daemon pid file\n"
"\t-o, --output-dir=<path>          where to store uploaded files\n"
"\n");
            printf(
"logging levels:\n"
"\t0         fatal errors, application cannot continue\n"
"\t1         major failure, needs immediate attention\n"
"\t2         critical errors\n"
"\t3         error but recoverable\n"
"\t4         warnings\n"
"\t5         normal message, but of high importance\n"
"\t6         info log, doesn't print that much (default)\n"
"\t7         debug, not needed in production\n"
"\n");
            printf(
"list types:\n"
"\t-1        blacklist mode, ips from list can NOT upload\n"
"\t 0        disable list (everyone can upload\n"
"\t 1        whitelist mode, only ips from list can upload\n");

            exit(0);

        case 'v':
            fprintf(stdout,
                    PACKAGE_STRING"\n"
                    "by Michał Łyszczek <michal.lyszczek@bofc.pl>\n\n");

            fprintf(stdout, "compilation options:\n\t");
            fprintf(stdout,
#if HAVE_SSL
                    "+"
#else
                    "-"
#endif
                    "ssl\n");

            exit(0);

        case ':':
            fprintf(stderr, "option -%c, --%s requires an argument\n",
                optopt, longopts[loptind].name);
            fprintf(stderr, "check --help for info about usage\n");
            return -1;

        case '?':
            fprintf(stderr, "unknown option %s\n", argv[optind - 1]);
            fprintf(stderr, "check --help for info about usage\n");
            return -1;

        default:
            fprintf(stderr, "unexpected return from getopt '0x%02x'\n", arg);
            fprintf(stderr, "check --help for info about usage\n");
            return -1;
        }
    }

    return 0;

#   undef PARSE_INT
#   undef PARSE_STR
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
    parses options from configuration file and command line arguments.
   ========================================================================== */


int config_init
(
    int    argc,   /* number of arguments in argv */
    char  *argv[]  /* argument list */
)
{
    /* disable error printing from getopt library */

    opterr = 0;

    /* set g_config object to well-known default state */

    memset(&g_config, 0, sizeof(g_config));

    g_config.log_level = EL_INFO;
    g_config.list_type = 0;
    g_config.colorful_output = 0;
    g_config.listen_port = 1337;
    g_config.timed_listen_port = 1338;
    g_config.ssl_listen_port = 0;
    g_config.timed_ssl_listen_port = 0;
    g_config.max_size = 1024 * 1024; /* 1MiB */
    g_config.daemonize = 0;
    g_config.max_connections = 10;
    g_config.max_timeout = 60;
    g_config.timed_max_timeout = 3;
    g_config.pem_pass_file[0] = '\0';
    strcpy(g_config.domain, "localhost");
    strcpy(g_config.bind_ip, "0.0.0.0");
    strcpy(g_config.user, "kurload");
    strcpy(g_config.group, "kurload");
    strcpy(g_config.query_log, "/var/log/kurload-query.log");
    strcpy(g_config.program_log, "/var/log/kurload.log");
    strcpy(g_config.pid_file, "/var/run/kurload.pid");
    strcpy(g_config.output_dir, "/var/lib/kurload");
    strcpy(g_config.list_file, "/etc/kurload/iplist");
    strcpy(g_config.key_file, "/etc/kurload/kurload.key");
    strcpy(g_config.cert_file, "/etc/kurload/kurload.cert");

    /* parse options from command line argument overwriting
     * default ones
     */

    if (config_parse_arguments(argc, argv) != 0)
    {
        return -1;
    }

    return 0;
}


/* ==========================================================================
    Performs some config validation. Should be called once embedlog is
    initialized.
   ========================================================================== */


int config_validate(void)
{
    /* check if we will be able to store uploaded files */

    if (access(g_config.output_dir, W_OK | X_OK) != 0)
    {
        el_perror(ELF, "output dir (%s) inaccessible",
                g_config.output_dir);
        return -1;
    }

    /* if list filtering is used, check if we can read IPs from it */

    if (g_config.list_type != 0)
    {
        if (access(g_config.list_file, R_OK) != 0)
        {
            el_perror(ELF, "list file (%s) unreadable",
                    g_config.list_file);
            return -1;
        }
    }

    /* if any of the ssl port is used, check if mandatory key and
     * cert files are accessible
     */

    if (g_config.ssl_listen_port != 0 || g_config.timed_ssl_listen_port != 0)
    {
        if (access(g_config.key_file, R_OK) != 0)
        {
            el_perror(ELF, "ssl key file (%s) unreadable",
                    g_config.key_file);
            return -1;
        }

        if (access(g_config.cert_file, R_OK) != 0)
        {
            el_perror(ELF, "ssl cert file (%s) unreadable",
                    g_config.cert_file);
            return -1;
        }
    }

    return 0;
}


/* ==========================================================================
    prints configuration to default logging facility
   ========================================================================== */


void config_print(void)
{
    /* macro for easy field printing */

#define CONFIG_PRINT(field, type) \
    el_print(ELI, "%s%s "type, #field, padder + strlen(#field), g_config.field)

    char padder[] = "....................:";
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    el_print(ELI, PACKAGE_STRING);
    el_print(ELI, "kurload configuration");
    CONFIG_PRINT(log_level, "%d");
    CONFIG_PRINT(colorful_output, "%ld");
    CONFIG_PRINT(listen_port, "%ld");
    CONFIG_PRINT(timed_listen_port, "%ld");
    CONFIG_PRINT(max_size, "%ld");
    CONFIG_PRINT(domain, "%s");
    CONFIG_PRINT(daemonize, "%ld");
    CONFIG_PRINT(max_connections, "%ld");
    CONFIG_PRINT(max_timeout, "%ld");
    CONFIG_PRINT(timed_max_timeout, "%ld");
    CONFIG_PRINT(user, "%s");
    CONFIG_PRINT(group, "%s");
    CONFIG_PRINT(query_log, "%s");
    CONFIG_PRINT(program_log, "%s");
    CONFIG_PRINT(list_file, "%s");
    CONFIG_PRINT(list_type, "%ld");
    CONFIG_PRINT(output_dir, "%s");
    CONFIG_PRINT(pid_file, "%s");
    CONFIG_PRINT(bind_ip, "%s");
#if HAVE_SSL
    CONFIG_PRINT(ssl_listen_port, "%ld");
    CONFIG_PRINT(timed_ssl_listen_port, "%ld");
    CONFIG_PRINT(key_file, "%s");
    CONFIG_PRINT(cert_file, "%s");
    CONFIG_PRINT(pem_pass_file, "%s");
#endif

#undef CONFIG_PRINT
}

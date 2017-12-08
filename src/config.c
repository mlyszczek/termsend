/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================

   -------------------------------------------------------------
  / This is configuration module, here we parse configuration   \
  | passed by the user. Configuration can be accessed by global |
  | config variable that is not modified outside this module.   |
  \ Never.                                                      /
   -------------------------------------------------------------
    \                                  ___-------___
     \                             _-~~             ~~-_
      \                         _-~                    /~-_
       \     /^\__/^\         /~  \                   /    \
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
   ========================================================================== */


/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "kurload.h"
#include "config.h"
#include "globals.h"

#include <confuse.h>
#include <ctype.h>
#include <embedlog.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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
    simply sets given value to specified option and makes sure  argument  is
    not too big
   ========================================================================== */


static void config_setstr
(
    const char  *option_argument,  /* really an optarg */
    const char  *option_name,      /* config name really */
    size_t       maxlen            /* maximum length of argument */
)
{
    if (strlen(option_argument) > maxlen)
    {
        fprintf(stderr, "value '%s' for '%s' is too long, max is %zu\n",
            option_argument, option_name, maxlen);
        exit(2);
    }

    cfg_setstr(g_config, option_name, option_argument);
}


/* ==========================================================================
    simple sets given integer value to specified option name and makes  sure
    argument value is between specified range
   ========================================================================== */


static void config_setint
(
    const char  *option_argument,  /* really an optarg */
    const char  *option_name,      /* config name really */
    long         max,              /* maximum valid value */
    long         min               /* minimum valid value */
)
{
    long         val;              /* value converted from option_argument */
    char        *endptr;           /* pointer for errors from strtol */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    val = strtol(option_argument, &endptr, 10);

    if (*endptr != '\0')
    {
        /*
         * error occured
         */

        fprintf(stderr, "wrong value '%s' for option '%s'\n",
            option_argument, option_name);
        exit(2);
    }

    if (val <= min || max <= val)
    {
        /*
         * number is outside of defined domain
         */

        fprintf(stderr, "value for '%s' should be between %ld and %ld\n",
            option_name, min, max);
        exit(2);
    }

    cfg_setint(g_config, option_name, val);
}


/* ==========================================================================
    parses arguments passed from command line and  overwrites  whatever  has
    been set in configuration file
   ========================================================================== */


static void config_parse_arguments
(
    int    argc,        /* number of arguments in argv */
    char  *argv[]       /* argument list */
)
{
    int                 arg;
    int                 loptind;
    static const char  *shortopts = ":hvl:cC:i:s:Dm:t:T:d:u:g:q:p:P:o:L:";
    struct option       longopts[] =
    {
        {"help",            no_argument,       NULL, 'h'},
        {"version",         no_argument,       NULL, 'v'},
        {"level",           required_argument, NULL, 'l'},
        {"colorful",        no_argument,       NULL, 'c'},
        {"config-file",     required_argument, NULL, 'C'},
        {"listen-port",     required_argument, NULL, 'i'},
        {"max-filesize",    required_argument, NULL, 's'},
        {"daemonize",       no_argument,       NULL, 'D'},
        {"max-connections", required_argument, NULL, 'm'},
        {"max-timeout",     required_argument, NULL, 't'},
        {"list-type",       required_argument, NULL, 'T'},
        {"domain",          required_argument, NULL, 'd'},
        {"user",            required_argument, NULL, 'u'},
        {"group",           required_argument, NULL, 'g'},
        {"query-log",       required_argument, NULL, 'q'},
        {"program-log",     required_argument, NULL, 'p'},
        {"pid-file",        required_argument, NULL, 'P'},
        {"output-dir",      required_argument, NULL, 'o'},
        {"ip-list",         required_argument, NULL, 'L'},
        {NULL, 0, NULL, 0}
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    optind = 0;
    while ((arg = getopt_long(argc, argv, shortopts, longopts, &loptind)) != -1)
    {
        switch (arg)
        {
        case 'h':
            fprintf(stdout,
"kurload - easy file sharing\n"
"\n"
"Usage: %s [-h | -v | options]\n"
"\n"
"options:\n"
"\t-h, --help                       prints this help and quits\n"
"\t-v, --version                    prints version and quits\n"
"\t-l, --level=<level>              logging level 0-7\n"
"\t-c, --colorful                   enable nice colors for logs\n"
"\t-C, --config-file=<path>         path for config (default /etc/kurload.conf)\n"
"\t-i, --listen-port=<port>         port on which program will listen\n"
"\t-s, --max-filesize=<size>        maximum size of file client can upload\n"
"\t-D, --daemonize                  run as daemon\n"
"\t-m, --max-connections=<number>   max number of concurrent connections\n"
"\t-t, --max-timeout=<seconds>      time before client is presumed dead\n"
"\t-T, --list-type=<type>           type of the ip-list (black or white list)\n"
"\t-d, --domain=<domain>            domain on which server works\n"
"\t-u, --user=<user>                user that should run daemon\n"
"\t-g, --group=<group>              group that should run daemon\n"
"\t-q, --query-log=<path>           where to store query logs\n"
"\t-p, --program-log=<path>         where to store program logs\n"
"\t-P, --pid-file=<path>            where to store daemon pid file\n"
"\t-o, --output-dir=<path>          where to store uploaded files\n"
"\t-L, --ip-list=<list>             list of ips to listen on\n"
"\n"
"logging levels:\n"
"\t0         fatal errors, application cannot continue\n"
"\t1         major failure, needs immediate attention\n"
"\t2         critical errors\n"
"\t3         error but recoverable\n"
"\t4         warnings\n"
"\t5         normal message, but of high importance\n"
"\t6         info log, doesn't print that much (default)\n"
"\t7         debug, not needed in production\n"
"\n"
"list types:\n"
"\t-1        blacklist mode, ips from list can NOT upload\n"
"\t 0        disable list (everyone can upload\n"
"\t 1        whitelist mode, only ips from list can upload\n",
argv[0]);

            exit(1);

        case 'v':
            fprintf(stdout,
                    PACKAGE_STRING"\n"
                    "by Michał Łyszczek <michal.lyszczek@bofc.pl>\n");

            exit(1);

        case 'c':
            cfg_setint(g_config, "colorful_output", 1);
            break;

        case 'D':
            cfg_setint(g_config, "daemonize", 1);
            break;

        case 'l':
            config_setint(optarg, "log_level", 0, 7);
            break;

        case 'i':
            config_setint(optarg, "listen_port", 0, UINT16_MAX);
            break;

        case 's':
            config_setint(optarg, "max_size", 0, LONG_MAX);
            break;

        case 'm':
            config_setint(optarg, "max_connections", 0, LONG_MAX);
            break;

        case 't':
            config_setint(optarg, "max_timeout", 1, LONG_MAX);
            break;

        case 'T':
            config_setint(optarg, "list_type", -1, 1);
            break;

        case 'd':
            config_setstr(optarg, "domain", UINT16_MAX);
            break;

        case 'u':
            config_setstr(optarg, "user", 256);
            break;

        case 'g':
            config_setstr(optarg, "group", 256);
            break;

        case 'q':
            config_setstr(optarg, "query_log", PATH_MAX);
            break;

        case 'p':
            config_setstr(optarg, "program_log", PATH_MAX);
            break;

        case 'P':
            config_setstr(optarg, "pid_file", PATH_MAX);
            break;

        case 'o':
            config_setstr(optarg, "output_dir", PATH_MAX);
            break;

        case 'L':
            break;

        case 'C':
            /*
             * we don't parse path to config file, as it was already parsed
             * earlier, and there is no point in parsing it here again.
             */
            break;

        case ':':
            fprintf(stderr, "option -%c, --%s requires an argument\n",
                optopt, longopts[loptind].name);
            exit(3);

        case '?':
            fprintf(stdout, "unknown option -%c\n", optopt);
            exit(1);

        default:
            fprintf(stderr, "unexpected return from getopt '0x%02x'\n", arg);
            exit(1);
        }
    }
}


/* ==========================================================================
    Creates program configuration with hardcoded values, then  reads  config
    file to overwrite any option defined in config file.  If config file  is
    not provided in command line argument, hardcoded  default  one  will  be
    used.
   ========================================================================== */


static void config_parse_configuration
(
    int    argc,                       /* number of command line arguments */
    char  *argv[]                      /* command line argument list */
)
{
    char   config_path[PATH_MAX + 1];  /* path to configuration file */
    int    opt;                        /* argument read from getopt_long */

    struct option longopts[] =
    {
        { "config-file", required_argument, NULL, 'C' },
        { NULL, 0, NULL, 0 }
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    strcpy(config_path, "/etc/kurload/kurload.conf");

    /*
     * check if location of config file has been overwriten with command line
     * argument
     */

    while ((opt = getopt_long(argc, argv, "C:", longopts, NULL)) != -1)
    {
        /*
         * we ignore any error here and look only for 'C' option
         */

        if (opt == 'C')
        {
            strncpy(config_path, optarg, PATH_MAX);
            config_path[PATH_MAX] = '\0';

            /*
             * config has been passed explicitly, we check if file exists
             */

            if (access(config_path, F_OK) != 0)
            {
                fprintf(stderr, "config file %s doesn't exist\n", config_path);
                exit(2);
            }

            break;
        }
    }

    /*
     * List of all configuration fields options in file with their default
     * values
     */

    cfg_opt_t options[] =
    {
    /*  type         field name          default value                 flags */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        CFG_STR_LIST("bind_ip",          "{0.0.0.0}",                    0),
        CFG_INT(     "log_level",        EL_INFO,                        0),
        CFG_INT(     "colorful_output",  0,                              0),
        CFG_INT(     "listen_port",      1337,                           0),
        CFG_INT(     "max_size",         1024 * 1024 /* 1MiB */,         0),
        CFG_INT(     "daemonize",        0,                              0),
        CFG_INT(     "max_connections",  10,                             0),
        CFG_INT(     "max_timeout",      60,                             0),
        CFG_INT(     "list_type",        0,                              0),
        CFG_STR(     "domain",           "localhost",                    0),
        CFG_STR(     "user",             "kurload",                      0),
        CFG_STR(     "group",            "kurload",                      0),
        CFG_STR(     "query_log",        "/var/log/kurload-query.log",   0),
        CFG_STR(     "program_log",      "/var/log/kurload.log",         0),
        CFG_STR(     "pid_file",         "/var/run/kurload.pid",         0),
        CFG_STR(     "output_dir",       "/var/lib/kurload",             0),
        CFG_STR(     "whitelist",        "/etc/kurload/whitelist",       0),
        CFG_STR(     "blacklist",        "/etc/kurload/blacklist",       0),
        CFG_END()
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if ((g_config = cfg_init(options, 0)) == NULL)
    {
        fprintf(stdout, "couldn't allocate memory for config parse\n");
        exit(2);
    }

    if (cfg_parse(g_config, config_path) != 0)
    {
        if (errno = ENOENT)
        {
            /*
             * if config file is specified via command line, file existance
             * is already checkout out, and if we use default configuration,
             * we don't fail as this is normal use case - like passing some
             * options via command line and accepting default options for
             * rest options
             */

            return;
        }

        fprintf(stdout, "parsing %s error %s\n", config_path, strerror(errno));
        cfg_free(g_config);
        exit(2);
    }
}


/* ==========================================================================
                                        __     __ _
                         ____   __  __ / /_   / /(_)_____
                        / __ \ / / / // __ \ / // // ___/
                       / /_/ // /_/ // /_/ // // // /__
                      / .___/ \__,_//_.___//_//_/ \___/
                     /_/
               ____                     __   _
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


/* ==========================================================================
    Parses configuration from command line and file and stores it in  global
    config variable.  Options from command line arguments will overwrite any
    option in configuration file.   Function  is  crucial  for  working,  if
    anything here fails, whole program will go dawn.
   ========================================================================== */


void config_init
(
    int    argc,   /* number of arguments in argv */
    char  *argv[]  /* argument list */
)
{
    opterr = 0;
    config_parse_configuration(argc, argv);
    config_parse_arguments(argc, argv);
}


/* ==========================================================================
    destroy config object and frees memory allocated by it.
   ========================================================================== */


void config_destroy(void)
{
   cfg_free(g_config);
}


/* ==========================================================================
    prints configuration to default logging facility
   ========================================================================== */


void config_print(void)
{
    /*
     * macros for easy field printing
     */

#define CONFIG_PRINT_STR(field) \
    el_print(ELI, "%s%s %s", #field, padder + strlen(#field), \
        cfg_getstr(g_config, #field));

#define CONFIG_PRINT_INT(field) \
    el_print(ELI, "%s%s %d", #field, padder + strlen(#field), \
        cfg_getint(g_config, #field));


    char padder[] = "....................:";
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    el_print(ELI, PACKAGE_STRING);
    el_print(ELI, "kurload configuration");
    CONFIG_PRINT_INT(log_level);
    CONFIG_PRINT_INT(colorful_output);
    CONFIG_PRINT_INT(listen_port);
    CONFIG_PRINT_INT(max_size);
    CONFIG_PRINT_STR(domain);
    CONFIG_PRINT_INT(daemonize);
    CONFIG_PRINT_INT(max_connections);
    CONFIG_PRINT_INT(max_timeout);
    CONFIG_PRINT_STR(user);
    CONFIG_PRINT_STR(group);
    CONFIG_PRINT_STR(query_log);
    CONFIG_PRINT_STR(program_log);
    CONFIG_PRINT_STR(whitelist);
    CONFIG_PRINT_STR(blacklist);
    CONFIG_PRINT_INT(list_type);
    CONFIG_PRINT_STR(output_dir);
    CONFIG_PRINT_STR(pid_file);
    CONFIG_PRINT_STR(bind_ip);

#undef CONFIG_PRINT_STR
#undef CONFIG_PRINT_INT
}

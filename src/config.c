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
#include "valid.h"

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
    parses arguments passed from command line and  overwrites  whatever  has
    been set in configuration file
   ========================================================================== */


static int config_parse_arguments
(
    int    argc,        /* number of arguments in argv */
    char  *argv[]       /* argument list */
)
{
    /*
     * macros to parse arguments in switch(opt) block
     */


    /*
     * check if optarg is between MINV and MAXV  values  and  if  so,  store
     * converted optarg in to config.OPTNAME field.  If error occurs,  force
     * function to return with -1 error
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
            /*                                                                 \
             * error occured                                                   \
             */                                                                \
                                                                               \
            fprintf(stderr, "wrong value '%s' for option '%s\n",               \
                optarg, #OPTNAME);                                             \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        if (val <= (long)MINV || (long)MAXV <= val)                            \
        {                                                                      \
            /*                                                                 \
             * number is outside of defined domain                             \
             */                                                                \
                                                                               \
            fprintf(stderr, "value for '%s' should be between %ld and %ld\n",  \
                #OPTNAME, (long)MINV, (long)MAXV);                             \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        g_config.OPTNAME = val;                                                \
    }


    /*
     * check if string length of optarg is less or equal than
     * sizeof(config.OPTNAME) and if so store optarg in config.OPTNAME.
     * If error occurs, force  function  to return with -1 error.
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
        case 'c': g_config.colorful_output = 1; break;
        case 'D': g_config.daemonize = 1; break;
        case 'l': PARSE_INT(log_level, 0, 7); break;
        case 'i': PARSE_INT(listen_port, 0, UINT16_MAX); break;
        case 's': PARSE_INT(max_size, 0, LONG_MAX); break;
        case 'm': PARSE_INT(max_connections, 0, LONG_MAX); break;
        case 't': PARSE_INT(max_timeout, 1, LONG_MAX); break;
        case 'T': PARSE_INT(list_type, -1, 1); break;
        case 'd': PARSE_STR(domain); break;
        case 'u': PARSE_STR(user); break;
        case 'g': PARSE_STR(group); break;
        case 'q': PARSE_STR(query_log); break;
        case 'p': PARSE_STR(program_log); break;
        case 'P': PARSE_STR(pid_file); break;
        case 'o': PARSE_STR(output_dir); break;
        case 'L': PARSE_STR(list_file); break;

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

        case 'C':
            /*
             * we don't parse path to config file, as it was already parsed
             * earlier, and there is no point in parsing it here again.
             */
            break;

        case ':':
            fprintf(stderr, "option -%c, --%s requires an argument\n",
                optopt, longopts[loptind].name);
            return -1;

        case '?':
            fprintf(stdout, "unknown option -%c\n", optopt);
            return -1;

        default:
            fprintf(stderr, "unexpected return from getopt '0x%02x'\n", arg);
            return -1;
        }
    }

    return 0;

#   undef PARSE_INT
#   undef PARSE_STR
}


/* ==========================================================================
    Creates program configuration with hardcoded values, then  reads  config
    file to overwrite any option defined in config file.  If config file  is
    not provided in command line argument, hardcoded  default  one  will  be
    used.
   ========================================================================== */


static int config_parse_configuration
(
    int    argc,                       /* number of command line arguments */
    char  *argv[]                      /* command line argument list */
)
{
    /*
     * macros for copying data from confuse object to our simple struct
     */


    /*
     * copy integer and validate if value is between MINV and MAXV, on
     * error macro sets rc to -1
     */


#   define COPY_INT(OPTNAME, MINV, MAXV)                                       \
    {                                                                          \
        long  val;                                                             \
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/   \
                                                                               \
        val = cfg_getint(cfg, #OPTNAME);                                       \
                                                                               \
        if (val < (long)MINV || (long)MAXV < val)                              \
        {                                                                      \
            fprintf(stderr, "value '%ld' for option '%s' should be bigger than"\
                "%ld and less than %ld, in config file %s\n",                  \
                val, #OPTNAME, (long)MINV, (long)MAXV, config_path);           \
            rc = -1;                                                           \
        }                                                                      \
    }


    /*
     * copy  string  and  validate  if  length  of  string  is  less   than
     * sizeof(g_config.OPTNAME)  -  1.   On  error  macro  sets  rc  to  -1
     */


#   define COPY_STR(OPTNAME)                                                   \
    {                                                                          \
        const char  *val;                                                      \
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/   \
                                                                               \
        val = cfg_getstr(cfg, #OPTNAME);                                       \
                                                                               \
        if (strlen(val) >= sizeof(g_config.OPTNAME))                           \
        {                                                                      \
            fprintf(stderr, "string length of option '%s' should be less"      \
                "than %zu \n", #OPTNAME, sizeof(g_config.OPTNAME));            \
            rc = -1;                                                           \
        }                                                                      \
    }


    char    config_path[PATH_MAX + 1];  /* path to configuration file */
    int     opt;                        /* argument read from getopt_long */
    int     rc;                         /* return code of function */
    cfg_t  *cfg;                        /* confuse object */


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
                fprintf(stderr, "couldn't access config file %s: %s\n",
                    config_path, strerror(errno));
                return -1;
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
    /*  type    field name          default value                   flags */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        CFG_INT("log_level",        EL_INFO,                        0),
        CFG_INT("colorful_output",  0,                              0),
        CFG_INT("listen_port",      1337,                           0),
        CFG_INT("max_size",         1024 * 1024 /* 1MiB */,         0),
        CFG_INT("daemonize",        0,                              0),
        CFG_INT("max_connections",  10,                             0),
        CFG_INT("max_timeout",      60,                             0),
        CFG_INT("list_type",        0,                              0),
        CFG_STR("bind_ip",          "0.0.0.0",                      0),
        CFG_STR("domain",           "localhost",                    0),
        CFG_STR("user",             "kurload",                      0),
        CFG_STR("group",            "kurload",                      0),
        CFG_STR("query_log",        "/var/log/kurload-query.log",   0),
        CFG_STR("program_log",      "/var/log/kurload.log",         0),
        CFG_STR("pid_file",         "/var/run/kurload.pid",         0),
        CFG_STR("output_dir",       "/var/lib/kurload",             0),
        CFG_STR("list_file",        "/etc/kurload/whitelist",       0),
        CFG_END()
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * parse configuration file and overwrite default parameters.
     */


    if ((cfg = cfg_init(options, 0)) == NULL)
    {
        fprintf(stdout, "couldn't allocate memory for config parse: %s\n",
            strerror(errno));
        return -1;
    }

    if (cfg_parse(cfg, config_path) != 0)
    {
        if (errno == ENOENT)
        {
            /*
             * if config file is specified via command line, file  existance
             * is already checkout out and this error will not occur in such
             * case.  But if we use default configuration  and  config  file
             * doesn't exist we don't fail as this is normal use case - like
             * passing some options via command line and  accepting  default
             * options for rest options
             */

            return 0;
        }

        fprintf(stdout, "parsing %s error %s\n", config_path, strerror(errno));
        cfg_free(cfg);
        return -1;
    }

    /*
     * move data from confuse to our struct  for  easier  access.   Not  the
     * fastest operation, but done only once at startup. If any of the COPY_
     * macro fails to copy data, rc will be set to -1 and function will fail
     * with -1 error
     */

    rc = 0;
    COPY_INT(log_level, 0, 7);
    COPY_INT(colorful_output, 0, 1);
    COPY_INT(listen_port, 0, UINT16_MAX);
    COPY_INT(max_size, 0, LONG_MAX);
    COPY_INT(daemonize, 0, 1);
    COPY_INT(max_connections, 0, LONG_MAX);
    COPY_INT(max_timeout, 1, LONG_MAX);
    COPY_INT(list_type, -1, 1);
    COPY_STR(bind_ip);
    COPY_STR(domain);
    COPY_STR(user);
    COPY_STR(group);
    COPY_STR(query_log);
    COPY_STR(program_log);
    COPY_STR(pid_file);
    COPY_STR(output_dir);
    COPY_STR(list_file);

    /*
     * free cfg as we don't need confuse anymore
     */

    cfg_free(cfg);
    return rc;
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
    parses options from configuration file and command line arguments.
   ========================================================================== */


int config_init
(
    int    argc,   /* number of arguments in argv */
    char  *argv[]  /* argument list */
)
{
    int   rv;      /* return value */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * disable error printing from getopt library
     */

    opterr = 0;

    /*
     * first we parse configuration from file
     */

    rv = config_parse_configuration(argc, argv);

    /*
     * then we parse options from command line argument overwriting
     * already parsed options from config file
     */

    rv |= config_parse_arguments(argc, argv);

    return rv;
}


/* ==========================================================================
    prints configuration to default logging facility
   ========================================================================== */


void config_print(void)
{
    /*
     * macros for easy field printing
     */

#define CONFIG_PRINT(field, type) \
    el_print(ELI, "%s%s "type, #field, padder + strlen(#field), g_config.field)

    char padder[] = "....................:";
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    el_print(ELI, PACKAGE_STRING);
    el_print(ELI, "kurload configuration");
    CONFIG_PRINT(log_level, "%ld");
    CONFIG_PRINT(colorful_output, "%ld");
    CONFIG_PRINT(listen_port, "%ld");
    CONFIG_PRINT(max_size, "%ld");
    CONFIG_PRINT(domain, "%s");
    CONFIG_PRINT(daemonize, "%ld");
    CONFIG_PRINT(max_connections, "%ld");
    CONFIG_PRINT(max_timeout, "%ld");
    CONFIG_PRINT(user, "%s");
    CONFIG_PRINT(group, "%s");
    CONFIG_PRINT(query_log, "%s");
    CONFIG_PRINT(program_log, "%s");
    CONFIG_PRINT(list_file, "%s");
    CONFIG_PRINT(list_type, "%ld");
    CONFIG_PRINT(output_dir, "%s");
    CONFIG_PRINT(pid_file, "%s");
    CONFIG_PRINT(bind_ip, "%s");

#undef CONFIG_PRINT
}

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


#include "config.h"
#include "version.h"

#include <confuse.h>
#include <embedlog.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>


/* ==========================================================================
                               __        __            __
                       ____ _ / /____   / /_   ____ _ / /
                      / __ `// // __ \ / __ \ / __ `// /
                     / /_/ // // /_/ // /_/ // /_/ // /
                     \__, //_/ \____//_.___/ \__,_//_/
                    /____/
                                   _         __     __
              _   __ ____ _ _____ (_)____ _ / /_   / /___   _____
             | | / // __ `// ___// // __ `// __ \ / // _ \ / ___/
             | |/ // /_/ // /   / // /_/ // /_/ // //  __/(__  )
             |___/ \__,_//_/   /_/ \__,_//_.___//_/ \___//____/

   ========================================================================== */


struct cfg_t  *g_config;  /* program configuration */


/* ==========================================================================
                                   _                __
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/

                   ____ ___   ____ _ _____ _____ ____   _____
                  / __ `__ \ / __ `// ___// ___// __ \ / ___/
                 / / / / / // /_/ // /__ / /   / /_/ /(__  )
                /_/ /_/ /_/ \__,_/ \___//_/    \____//____/

   ========================================================================== */


/* ==========================================================================
    macros for easy field printing
   ========================================================================== */


#define CONFIG_PRINT_STR(field) \
    el_print(ELI, "%-20s %s", #field, cfg_getstr(g_config, #field))

#define CONFIG_PRINT_INT(field) \
    el_print(ELI, "%-20s %d", #field, cfg_getint(g_config, #field))


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
    Parses command line arguments, if error is detected, program will commit
    suicide. Arguments values are returned via function arguments, it's only
    3 of them, no biggie.
   ========================================================================== */


static void config_parse_arguments
(
    int    argc,        /* number of arguments in argv */
    char  *argv[],      /* argument list */
    int   *color,       /* colors enabled in argument? */
    int   *level,       /* log level from arguments */
    char  *config_path  /* configuration path from arguments */
)
{
    int    arg;         /* argument "name" */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    opterr = 0;
    *color = -1;
    *level = -1;
    strcpy(config_path, "/etc/kurload/kurload.conf");

    /*
     * first we parse command line arguments
     */

    while ((arg = getopt(argc, argv, "hvcl:f:")) != -1)
    {
        switch (arg)
        {
        case 'h':
            fprintf(stdout,
                    "kurload - easy file sharing\n"
                    "\n"
                    "Usage: %s [-h | -v | -c -l<level> -f<path>]\n"
                    "\n"
                    "\t-h         prints this help and quits\n"
                    "\t-v         prints version and quits\n"
                    "\t-c         if set, output will have nice colors\n"
                    "\t-l<num>    logging level 0-3 \n"
                    "\t-f<path>   path to configuration file\n"
                    "\n"
                    "logging level\n"
                    "\t0          log only critical error messages\n"
                    "\t1          log also non-critical warning messages\n"
                    "\t2          verbose output\n"
                    "\t3          debug verbose, prints a lot\n",
                    argv[0]);

            exit(1);

        case 'v':
            fprintf(stdout,
                    APP_VERSION"\n"
                    "kurload by Michał Łyszczek <michal.lyszczek@bofc.pl>\n");

            exit(1);

        case 'c':
            *color = 1;
            break;

        case 'l':
            /*
             * convert level char to int
             */

            *level = optarg[0] - '0';

            if (*level < 0 || 3 < *level)
            {
                /*
                 * level is outside accepted values
                 */

                fprintf(stdout, "invalid log level %c\n", optarg[0]);
                exit(2);
            }

            break;

        case 'f':
            strncpy(config_path, optarg, PATH_MAX);
            config_path[PATH_MAX] = '\0';
            break;

        case '?':
            if (optopt == 'l' || optopt == 'f')
            {
                fprintf(stdout, "option -%c requires an argument\n", optopt);
            }
            else if (isprint(optopt))
            {
                fprintf(stdout, "unknown option -%c\n", optopt);
            }
            else
            {
                fprintf(stdout, "unknown option character '0x%02x'\n", optopt);
            }

        default:
            exit(1);
        }
    }
}


static void config_parse_configuration
(
    char  *config_path  /* configuration file - it is sure to be set */
)
{
    /*
     * List of all configuration fields options in file with their default
     * values
     */

    cfg_opt_t options[] =
    {
    /*  type    field name          default value                   flags */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        CFG_INT("log_level",        EL_LEVEL_ERR,                   0),
        CFG_INT("colorful_output",  0,                              0),
        CFG_INT("listen_port",      1337,                           0),
        CFG_INT("max_size",         1024 * 1024 /* 1MiB */,         0),
        CFG_STR("domain",           "localhost",                    0),
        CFG_STR("user",             "kurload",                      0),
        CFG_STR("group",            "kurload",                      0),
        CFG_STR("query_log",        "/var/log/kurload-query.log",   0),
        CFG_STR("program_log",      "/var/log/kurload.log",         0),
        CFG_STR("whitelist",        "/etc/kurload/whitelist",       0),
        CFG_STR("blacklist",        "/etc/kurload/blacklist",       0),
        CFG_STR("list_type",        "none",                         0),
        CFG_STR("output_dir",       "/var/lib/kurload",             0),
        CFG_STR("bind_if",          "{none}",                       CFGF_LIST),
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
    int    argc,                       /* number of arguments in argv */
    char  *argv[]                      /* argument list */
)
{
    int    color;                      /* enable colorful output */
    int    level;                      /* logging level */
    char   config_path[PATH_MAX + 1];  /* path to configuration file */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    config_parse_arguments(argc, argv, &color, &level, config_path);
    config_parse_configuration(config_path);

    /*
     * configuration file parsed, overwrite any option that was passed by
     * command line arguments
     */

    if (color != -1)
    {
        cfg_setint(g_config, "colorful_output", color);
    }

    if (level != -1)
    {
        cfg_setint(g_config, "log_level", level);
    }
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
    el_print(ELI, APP_VERSION);
    el_print(ELI, "kurload configuration");
    CONFIG_PRINT_INT(log_level);
    CONFIG_PRINT_INT(colorful_output);
    CONFIG_PRINT_INT(listen_port);
    CONFIG_PRINT_INT(max_size);
    CONFIG_PRINT_STR(domain);
    CONFIG_PRINT_STR(user);
    CONFIG_PRINT_STR(group);
    CONFIG_PRINT_STR(query_log);
    CONFIG_PRINT_STR(program_log);
    CONFIG_PRINT_STR(whitelist);
    CONFIG_PRINT_STR(blacklist);
    CONFIG_PRINT_STR(list_type);
    CONFIG_PRINT_STR(output_dir);
    CONFIG_PRINT_STR(bind_if);
}

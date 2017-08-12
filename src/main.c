/* ==========================================================================
    Licensed under BSD 2clause license. See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================

   ------------------------------------------------------------
  / This is entry point of server, here things are initialized \
  \ and main loop is started                                   /
   ------------------------------------------------------------
    \
     \ \_\_    _/_/
      \    \__/
           (oo)\_______
           (__)\       )\/\
               ||----w |
               ||     ||
   ========================================================================== */


/* ==========================================================================
      _               __            __           __   ____ _  __
     (_)____   _____ / /__  __ ____/ /___   ____/ /  / __/(_)/ /___   _____
    / // __ \ / ___// // / / // __  // _ \ / __  /  / /_ / // // _ \ / ___/
   / // / / // /__ / // /_/ // /_/ //  __// /_/ /  / __// // //  __/(__  )
  /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/ \__,_/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include <string.h>
#include <signal.h>
#include <confuse.h>
#include <embedlog.h>

#include "config.h"
#include "bnwlist.h"
#include "globals.h"
#include "server.h"


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


struct el_options  g_qlog;      /* options for embedlog to print query logs */
int                g_shutdown;  /* flag indicating that program should die */


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


static void sigint_handler(int signo)
{
    (void)signo;

    el_print(ELI, "received SIGINT, exiting");

    g_shutdown = 0;
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


int main(int argc, char *argv[])
{
    int               list_type;
    const char       *list_file;
    struct sigaction  sa;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    config_init(argc, argv);
    el_level_set(cfg_getint(g_config, "log_level"));
    el_output_enable(EL_OUT_STDERR);
    el_option(EL_OPT_TS, EL_OPT_TS_LONG);
    el_option(EL_OPT_FINFO, 1);
    el_option(EL_OPT_TS_TM, EL_OPT_TS_TM_REALTIME);
    el_option(EL_OPT_COLORS, cfg_getint(g_config, "colorful_output"));

    el_options_init(&g_qlog);
    el_olevel_set(&g_qlog, EL_LEVEL_INF);
    el_ooutput_enable(&g_qlog, EL_OUT_STDERR);
    el_ooption(&g_qlog, EL_OPT_TS, EL_OPT_TS_LONG);
    el_ooption(&g_qlog, EL_OPT_COLORS, 0);
    el_ooption(&g_qlog, EL_OPT_FINFO, 0);
    el_ooption(&g_qlog, EL_OPT_TS_TM, EL_OPT_TS_TM_REALTIME);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    config_print();

    list_type = cfg_getint(g_config, "list_type");
    list_file = cfg_getstr(g_config, list_type == 1 ? "whitelist" : "blacklist");

    bnw_init(list_file, list_type);

    if (server_init() != 0)
    {
        el_print(ELE, "couldn't start server, aborting");
        goto server_error;
    }

    server_loop_forever();

server_error:
error:
    server_destroy();
    bnw_destroy();
    config_destroy();

    return 0;
}

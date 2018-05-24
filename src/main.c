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


#include <errno.h>
#include <string.h>
#include <signal.h>
#include <embedlog.h>

#include "config.h"
#include "bnwlist.h"
#include "globals.h"
#include "server.h"
#include "daemonize.h"


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
    handles SIGINT and SIGTERM signals,  it  basicly  sets  global  variable
    g_shutdown to 1, to inform all module to exit.  Also accept() from  main
    loop will receive EINTR so it doesn't stuck
   ========================================================================== */


static void sigint_handler(int signo)
{
    (void)signo;

    if (g_shutdown)
    {
        /*
         * someone hit ctrl-c second time, impatient fella
         */

        el_print(ELI, "second SIGINT, ok... got it, will deal with it!");
        g_stfu = 1;
        return;
    }

    el_print(ELI, "received SIGINT, exiting");
    g_shutdown = 1;
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
    int               rv;
    struct sigaction  sa;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if (config_init(argc, argv) != 0)
    {
        fprintf(stderr, "fatal error parsing configuration, cannot continue\n");
        return 1;
    }

    if (g_config.daemonize)
    {
        daemonize(g_config.pid_file, g_config.user, g_config.group);
    }

    /*
     * configure logger for diagnostic logs
     */

    el_init();
    el_option(EL_LEVEL, g_config.log_level);
    el_option(EL_OUT, EL_OUT_FILE);
    el_option(EL_TS, EL_TS_LONG);
    el_option(EL_TS_TM, EL_TS_TM_REALTIME);
    el_option(EL_FINFO, 1);
    el_option(EL_COLORS, g_config.colorful_output);

    if (el_option(EL_FPATH, g_config.program_log) != 0)
    {
        fprintf(stderr, "WARNING couldn't open program log file %s: %s "
            "logs will be printed to stderr\n",
            g_config.program_log,  strerror(errno));
        el_option(EL_OUT, EL_OUT_STDERR);
    }

    /*
     * configure logger to log queries
     */

    el_oinit(&g_qlog);
    el_ooption(&g_qlog, EL_LEVEL, EL_INFO);
    el_ooption(&g_qlog, EL_OUT, EL_OUT_FILE);
    el_ooption(&g_qlog, EL_TS, EL_TS_LONG);
    el_ooption(&g_qlog, EL_TS_TM, EL_TS_TM_REALTIME);
    el_ooption(&g_qlog, EL_PRINT_LEVEL, 0);
    el_ooption(&g_qlog, EL_FILE_SYNC_EVERY, 0);

    if (el_ooption(&g_qlog, EL_FPATH, g_config.query_log) != 0)
    {
        fprintf(stderr, "WARNING couldn't open query log file %s: %s\n",
             g_config.query_log, strerror(errno));
        el_ooption(&g_qlog, EL_OUT, EL_OUT_NONE);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    config_print();

    if (bnw_init(g_config.list_file, g_config.list_type) != 0)
    {
        rv = 1;
        el_print(ELE, "couldn't initialize list from file %s",
            g_config.list_file);
        goto bnw_error;
    }

    if (server_init() != 0)
    {
        rv = 1;
        el_print(ELE, "couldn't start server, aborting");
        goto server_error;
    }

    server_loop_forever();
    server_destroy();
    rv = 0;

server_error:
    bnw_destroy();

bnw_error:
    if (g_config.daemonize)
    {
        daemonize_cleanup(g_config.pid_file);
    }

    return rv;
}

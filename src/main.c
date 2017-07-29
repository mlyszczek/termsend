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


#include <confuse.h>
#include <embedlog.h>

#include "config.h"
#include "bnwlist.h"

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
    int          list_type;
    const char  *list_file;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    config_init(argc, argv);
    el_level_set(cfg_getint(g_config, "log_level"));
    el_output_enable(EL_OUT_STDERR);
    el_option(EL_OPT_TS, EL_OPT_TS_LONG);
    el_option(EL_OPT_FINFO, 1);
    el_option(EL_OPT_TS_TM, EL_OPT_TS_TM_REALTIME);
    el_option(EL_OPT_COLORS, cfg_getint(g_config, "colorful_output"));

    config_print();

    list_type = cfg_getint(g_config, "list_type");
    list_file = cfg_getstr(g_config, list_type == 1 ? "whitelist" : "blacklist");

    bnw_init(list_file, list_type);

error:
    bnw_destroy();
    config_destroy();

    return 0;
}

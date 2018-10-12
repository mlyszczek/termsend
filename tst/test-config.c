/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "config.h"
#include "globals.h"
#include "mtest.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


/* ==========================================================================
                                   _                __
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
                                   _         __     __
              _   __ ____ _ _____ (_)____ _ / /_   / /___   _____
             | | / // __ `// ___// // __ `// __ \ / // _ \ / ___/
             | |/ // /_/ // /   / // /_/ // /_/ // //  __/(__  )
             |___/ \__,_//_/   /_/ \__,_//_.___//_/ \___//____/

   ========================================================================== */


mt_defs_ext();
struct config  config;


/* ==========================================================================
                    _                __           __               __
      ____   _____ (_)_   __ ____ _ / /_ ___     / /_ ___   _____ / /_ _____
     / __ \ / ___// /| | / // __ `// __// _ \   / __// _ \ / ___// __// ___/
    / /_/ // /   / / | |/ // /_/ // /_ /  __/  / /_ /  __/(__  )/ /_ (__  )
   / .___//_/   /_/  |___/ \__,_/ \__/ \___/   \__/ \___//____/ \__//____/
  /_/
   ========================================================================== */


static void test_prepare(void)
{
    memset(&config, 0, sizeof(config));

    config.log_level = EL_INFO;
    config.list_type = 0;
    config.colorful_output = 0;
    config.listen_port = 1337;
    config.max_size = 1024 * 1024; /* 1MiB */
    config.daemonize = 0;
    config.max_connections = 10;
    config.max_timeout = 60;
    strcpy(config.domain, "localhost");
    strcpy(config.bind_ip, "0.0.0.0");
    strcpy(config.user, "kurload");
    strcpy(config.group, "kurload");
    strcpy(config.query_log, "/var/log/kurload-query.log");
    strcpy(config.program_log, "/var/log/kurload.log");
    strcpy(config.pid_file, "/var/run/kurload.pid");
    strcpy(config.output_dir, "/var/lib/kurload");
    strcpy(config.list_file, "/etc/kurload-iplist");
}


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


/* ==========================================================================
   ========================================================================== */


static void config_all_default(void)
{
    char  *argv[] = { "kurload" };
    config_init(1, argv);
    mt_fok(memcmp(&config, &g_config, sizeof(config)));
}


/* ==========================================================================
   ========================================================================== */


static void config_short_opts(void)
{
    char *argv[] =
    {
        "kurload",
        "-l4",
        "-c",
        "-i100",
        "-s512",
        "-D",
        "-m", "3",
        "-t20",
        "-T", "-1",
        "-dhttp://kurload.kurwinet.pl",
        "-ukur",
        "-gload",
        "-q/query",
        "-p/program",
        "-P/pid",
        "-o", "/output",
        "-b0.0.0.0,1.3.3.7",
        "-L/iplist",
        "-U"
    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    config_init(argc, argv);

    config.log_level = 4;
    config.timed_upload = 1;
    config.list_type = -1;
    config.colorful_output = 1;
    config.daemonize = 1;
    config.listen_port = 100;
    config.max_size = 512;
    config.max_connections = 3;
    config.max_timeout = 20;
    strcpy(config.domain, "http://kurload.kurwinet.pl");
    strcpy(config.bind_ip, "0.0.0.0,1.3.3.7");
    strcpy(config.user, "kur");
    strcpy(config.group, "load");
    strcpy(config.query_log, "/query");
    strcpy(config.program_log, "/program");
    strcpy(config.pid_file, "/pid");
    strcpy(config.list_file, "/iplist");
    strcpy(config.output_dir, "/output");

    mt_fok(memcmp(&config, &g_config, sizeof(config)));
}


/* ==========================================================================
   ========================================================================== */


static void config_long_opts(void)
{
    char *argv[] =
    {
        "kurload",
        "--level=4",
        "--colorful-output",
        "--listen-port=100",
        "--max-filesize=512",
        "--daemonize",
        "--max-connections=3",
        "--max-timeout=20",
        "--list-type=-1",
        "--domain=http://kurload.kurwinet.pl",
        "--user=kur",
        "--group=load",
        "--query-log=/query",
        "--program-log=/program",
        "--pid-file=/pid",
        "--output-dir=/output",
        "--bind-ip=0.0.0.0,1.3.3.7",
        "--timed-upload"
    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    config_init(argc, argv);

    config.log_level = 4;
    config.timed_upload = 1;
    config.list_type = -1;
    config.colorful_output = 1;
    config.daemonize = 1;
    config.listen_port = 100;
    config.max_size = 512;
    config.max_connections = 3;
    config.max_timeout = 20;
    strcpy(config.domain, "http://kurload.kurwinet.pl");
    strcpy(config.bind_ip, "0.0.0.0,1.3.3.7");
    strcpy(config.user, "kur");
    strcpy(config.group, "load");
    strcpy(config.query_log, "/query");
    strcpy(config.program_log, "/program");
    strcpy(config.pid_file, "/pid");
    strcpy(config.output_dir, "/output");

    mt_fok(memcmp(&config, &g_config, sizeof(config)));
}


/* ==========================================================================
   ========================================================================== */


static void config_mixed_opts(void)
{
    char *argv[] =
    {
        "kurload",
        "-l4",
        "--colorful-output",
        "--listen-port=100",
        "--max-filesize=512",
        "-D",
        "--max-connections=3",
        "--max-timeout=20",
        "--list-type=-1",
        "-d", "http://kurload.kurwinet.pl",
        "--user=kur",
        "--group=load",
        "-q/query",
        "-p", "/program",
        "--pid-file=/pid",
        "--output-dir=/output",
        "--list-file=/iplist",
        "--bind-ip=0.0.0.0,1.3.3.7"
    };
    int argc = sizeof(argv) / sizeof(const char *);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    config_init(argc, argv);

    config.log_level = 4;
    config.list_type = -1;
    config.colorful_output = 1;
    config.daemonize = 1;
    config.listen_port = 100;
    config.max_size = 512;
    config.max_connections = 3;
    config.max_timeout = 20;
    strcpy(config.domain, "http://kurload.kurwinet.pl");
    strcpy(config.bind_ip, "0.0.0.0,1.3.3.7");
    strcpy(config.user, "kur");
    strcpy(config.group, "load");
    strcpy(config.query_log, "/query");
    strcpy(config.program_log, "/program");
    strcpy(config.pid_file, "/pid");
    strcpy(config.output_dir, "/output");
    strcpy(config.list_file, "/iplist");

    mt_fok(memcmp(&config, &g_config, sizeof(config)));
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void config_test_group()
{
    mt_prepare_test = &test_prepare;

    mt_run(config_all_default);
    mt_run(config_short_opts);
    mt_run(config_long_opts);
    mt_run(config_mixed_opts);
}

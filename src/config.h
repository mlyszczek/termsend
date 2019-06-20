/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


#ifndef APP_CONFIG_H
#define APP_CONFIG_H 1

#include "kurload.h"

#include <embedlog.h>
#include <limits.h>

#if HAVE_LINUX_LIMITS_H
#   include <linux/limits.h>
#endif

enum list_type
{
    list_type_black = -1,
    list_type_none  =  0,
    list_type_white =  1
};


struct config
{
    enum el_level   log_level;
    int             timed_upload;
    long            list_type;
    long            colorful_output;
    long            listen_port;
    long            ssl_listen_port;
    long            max_size;
    long            daemonize;
    long            max_connections;
    long            max_timeout;
    char            domain[4096 + 1];
    char            bind_ip[1024 + 1];
    char            user[255 + 1];
    char            group[255 + 1];
    char            query_log[PATH_MAX + 1];
    char            program_log[PATH_MAX + 1];
    char            pid_file[PATH_MAX + 1];
    char            output_dir[PATH_MAX + 1];
    char            list_file[PATH_MAX + 1];
    char            key_file[PATH_MAX + 1];
    char            cert_file[PATH_MAX + 1];
    char            pem_pass_file[PATH_MAX + 1];
};


int config_init(int argc, char *argv[]);
void config_destroy(void);
void config_print(void);


#endif

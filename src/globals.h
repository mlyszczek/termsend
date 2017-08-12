/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef GLOBALS_H
#define GLOBALS_H 1

#include <confuse.h>
#include <embedlog.h>

extern cfg_t             *g_config;
extern int                g_shutdown;
extern struct el_options  g_qlog;

#endif

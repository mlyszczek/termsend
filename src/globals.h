/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef GLOBALS_H
#define GLOBALS_H 1

#include "config.h"
#include <embedlog.h>

extern struct config  g_config;
extern int            g_shutdown;
extern int            g_stfu;
extern struct el      g_qlog;

#endif

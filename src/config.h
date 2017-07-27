/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


#ifndef APP_CONFIG_H
#define APP_CONFIG_H 1


#include <confuse.h>

extern cfg_t *g_config;

void config_init(int, char **);
void config_destroy(void);
void config_print(void);


#endif

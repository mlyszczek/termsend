/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


#ifndef APP_CONFIG_H
#define APP_CONFIG_H 1


#include <confuse.h>


int config_init(int argc, char *argv[]);
void config_destroy(void);
void config_print(void);


#endif

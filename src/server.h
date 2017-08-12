/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef SERVER_H
#define SERVER_H 1

#include <arpa/inet.h>

int server_init(void);
void server_loop_forever(void);

#endif

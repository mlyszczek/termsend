/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef DAEMONIZE_H
#define DAEMONIZE_H 1

void daemonize(const char *, const char *, const char *);
void daemonize_cleanup(const char *);

#endif

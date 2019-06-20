/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef KURLOAD_SSL_H
#define KURLOAD_SSL_H 1

#include <sys/types.h>

int ssl_init(void);
int ssl_cleanup(void);

int ssl_accept(int cfd);
int ssl_close(int ssl_fd);
int ssl_shutdown(int ssl_fd, int how);
ssize_t ssl_write(int ssl_fd, const void *buf, size_t count);
ssize_t ssl_read(int ssl_fd, void *buf, size_t count);

#endif

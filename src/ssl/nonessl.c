/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "ssl.h"
#include <errno.h>


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
   ========================================================================== */


int ssl_init
(
    void
)
{
    errno = ENOSYS;
    return -1;
}


/* ==========================================================================
   ========================================================================== */


int ssl_cleanup
(
    void
)
{
    errno = ENOSYS;
    return -1;
}


/* ==========================================================================
   ========================================================================== */


int ssl_accept
(
    int  cfd
)
{
    (void)cfd;

    errno = ENOSYS;
    return -1;
}

/* ==========================================================================
   ========================================================================== */


int ssl_close
(
    int  ssl_fd
)
{
    (void)ssl_fd;

    errno = ENOSYS;
    return -1;
}


/* ==========================================================================
   ========================================================================== */


ssize_t ssl_write
(
    int          ssl_fd,
    const void  *buf,
    size_t       count
)
{
    (void)ssl_fd;
    (void)buf;
    (void)count;

    errno = ENOSYS;
    return -1;
}

/* ==========================================================================
   ========================================================================== */


ssize_t ssl_read
(
    int      ssl_fd,
    void    *buf,
    size_t   count
)
{
    (void)ssl_fd;
    (void)buf;
    (void)count;

    errno = ENOSYS;
    return -1;
}

/* ==========================================================================
   ========================================================================== */


int ssl_shutdown
(
    int  ssl_fd,
    int  how
)
{
    (void)ssl_fd;
    (void)how;

    errno = ENOSYS;
    return -1;
}
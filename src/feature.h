/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

/* these are definitions for feature test macros, keep that file
 * included in every *.c file at top.
 */

#if HAVE_CONFIG_H
#   include "termsend.h"
#endif


/* needed by htonl() function familly
 */

#define _POSIX_C_SOURCE 200112L

/* on freebsd INADDR_NONE is not visible without __BSD_VISIBLE
 */

#if __FreeBSD__
#   define __BSD_VISIBLE 1
#endif

/* on netbsd ntohl() is not visible without _NETBSD_SOURCE
 */

#if __NetBSD__
#   define _NETBSD_SOURCE 1
#endif

/* qnx requires _QNX_SOURCE for PATH_MAX to be defined
 */

#if __QNX__ || __QNXNTO__
#   define _QNX_SOURCE 1
#endif

/* struct timeval on solaris needs __EXTENSIONS__
 */

#if sun || __sun
#   define __EXTENSIONS__ 1
#endif

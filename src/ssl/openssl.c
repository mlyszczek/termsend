/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "ssl.h"

#ifdef HAVE_CONFIG_H
#   include "kurload.h"
#endif

#include <embedlog.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "globals.h"
#include "valid.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


static SSL_CTX  *g_ctx;
static SSL     **g_ssl;


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Prints earliest openssl error. If ssl_fd is -1, ssl_ret is ignored and
    error for ssl_err will not be printed.
   ========================================================================== */


void print_openssl_error
(
    int ssl_fd,               /* current ssl socket in use */
    int ssl_ret               /* return value from ssl function */
)
{
    char           msg[128];  /* human readable error message */
    unsigned long  err;       /* openssl error */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (ssl_fd != -1)
    {
        /* get ssl error (from accept() write() etc */

        err = SSL_get_error(g_ssl[ssl_fd], ssl_ret);
        ERR_error_string(err, msg);
        el_print(ELE, "openssl ssl_get_error: %s", msg);
    }

    /* print every error in openssl's error queue */

    while ((err = ERR_get_error()) != 0)
    {
        ERR_error_string(err, msg);
        el_print(ELE, "openssl err_get_error: %s", msg);
    }
}


/* ==========================================================================
    Prints error message returned from functions that use SSL object, like
    SSL_write() or SSL_read().
   ========================================================================== */


void print_openssl_ssl_error
(
    int            ret,
    int            ssl_index
)
{
    char           msg[128];  /* human readable error message */
    unsigned long  err;       /* openssl error */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    err = SSL_get_error(g_ssl[ssl_index], ret);
    ERR_error_string(err, msg);
    el_print(ELE, "openssl: %s", msg);
}


/* ==========================================================================
    Reads password for key and hands it back to openssl.
   ========================================================================== */


int pem_password_callback
(
    char         *buf,      /* buffer where password should be stored */
    int           size,     /* size of buf buffer */
    int           rwflag,   /* 0 - pass for decryption, 1 pass for encryption */
    void         *userdata  /* user data, not used here */
)
{
    int          r;         /* return from read() function */
    int          tr;        /* total bytes read from file */
    int          fd;        /* password file handle */
    struct stat  st;        /* password file info */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    (void)rwflag;
    (void)userdata;

    if (stat(g_config.pem_pass_file, &st) != 0)
    {
        el_print(ELE, "failed to read password file");
        el_perror(ELE, "stat(%s)", g_config.pem_pass_file);
        return -1;
    }

    /* check if password can fit into buffer, -1 because we skip last
     * newline character
     */

    if (st.st_size - 1 > size)
    {
        el_print(ELE, "password in %s is too long, max is %d",
                g_config.pem_pass_file, size);
        return -1;
    }

    fd = open(g_config.pem_pass_file, O_RDONLY);
    if (fd < 0)
    {
        el_perror(ELE, "open(%s, O_RDONLY)", g_config.pem_pass_file);
        return -1;
    }

    for (tr = 0; tr < st.st_size - 1; tr += r)
    {
        r = read(fd, buf + tr, st.st_size - 1 - tr);

        if (r == -1)
        {
            el_print(ELE, "failed to read password file");
            el_perror(ELE, "read(%d, %p, %d)",
                    fd, buf + tr, st.st_size - 1 - tr);
            close(fd);
            return -1;
        }
    }

    close(fd);
    return tr;
}


/* ==========================================================================
    Returns index of unused ssl connection structure. -1 when all slots are
    taken.
   ========================================================================== */


int get_free_ssl
(
    void
)
{
    int  i;  /* loop iterator */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (i = 0; i != g_config.max_connections; ++i)
    {
        if (g_ssl[i] == NULL)
        {
            /* this slot is empty, return its index */

            return i;
        }

        /* slot used, keep searching */
    }

    /* all slots taken */

    return -1;
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
    Initializes openssl library. This function is called once at startup.
   ========================================================================== */


int ssl_init
(
    void
)
{
    int  i;    /* loop iterator */
    int  ret;  /* return from functions */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* allocate enough ssl structures for all possible connections */

    g_ssl = malloc(sizeof(*g_ssl) * g_config.max_connections);

    /* invalidate all ssl structores to know which one is used */

    for (i = 0; i != g_config.max_connections; ++i)
    {
        g_ssl[i] = NULL;
    }

    /* initialize openssl library, initialization is only needed in
     * version <1.1.0, in 1.1.0 this is done automatically in various
     * functions. Explicit initialization should be done only, when
     * we would want to initialize library in non default way - which
     * we do not need.
     */

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_load_error_strings();
    SSL_library_init();
#endif

    /* create context that will live for the entire life of program.
     * SSLv23_server_method() is a ssl method that will negotiate
     * highest available protocol supported by server and client.
     */

    g_ctx = SSL_CTX_new(SSLv23_server_method());
    if (g_ctx == NULL)
    {
        print_openssl_error(-1, 0);
        free(g_ssl);
        return -1;
    }

    if (SSL_CTX_set_ecdh_auto(g_ctx, 1) != 1)
    {
        print_openssl_error(-1, 0);
        free(g_ssl);
        SSL_CTX_free(g_ctx);
        return -1;
    }

    if (g_config.pem_pass_file[0] != '\0')
    {
        /* pem pass file is set, set callback to read password from
         * that file
         */

        SSL_CTX_set_default_passwd_cb(g_ctx, pem_password_callback);
    }

    /* set cert and key */

    ret = SSL_CTX_use_certificate_file(g_ctx, g_config.cert_file,
            SSL_FILETYPE_PEM);
    if (ret <= 0)
    {
        print_openssl_error(-1, 0);
        free(g_ssl);
        SSL_CTX_free(g_ctx);
        return -1;
    }

    ret = SSL_CTX_use_PrivateKey_file(g_ctx, g_config.key_file,
            SSL_FILETYPE_PEM);
    if (ret <= 0)
    {
        print_openssl_error(-1, 0);
        free(g_ssl);
        SSL_CTX_free(g_ctx);
        return -1;
    }

    el_print(ELN, "openssl initialized");
    return 0;
}


/* ==========================================================================
    Frees what has been allocated by ssl_init(). This is called once at the
    end of program.
   ========================================================================== */


int ssl_cleanup
(
    void
)
{
    SSL_CTX_free(g_ctx);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_cleanup();
#endif
    free(g_ssl);
    return 0;
}


/* ==========================================================================
    Perform ssl handshake. Note, this is not socket accept(2) function, but
    ssl library accept(3). This function is called each time, after socket
    accept(2) is done. When ssl_accept() is called, this means connection
    was accepted, is not blocked, and connection limit has not yet been
    reached.

    return
            >0      ssl file descriptor
            -1      error
   ========================================================================== */


int ssl_accept
(
    int  cfd    /* client fd as returned from accept(2) */
)
{
    int  slot;  /* free ssl slot */
    int  ret;   /* return from ssl_accept() */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    slot = get_free_ssl();
    if (slot < 0)
    {
        /* all slots are taken */

        errno = ENOSPC;
        return -1;
    }

    /* create ssls tructure for a connection */

    g_ssl[slot] = SSL_new(g_ctx);
    if (g_ssl[slot] == NULL)
    {
        print_openssl_error(-1, 0);
        return -1;
    }

    /* bind ssl object with accept(2)ed client's connection */

    SSL_set_fd(g_ssl[slot], cfd);

    /* wait for ssl handshake from client */

    ret = SSL_accept(g_ssl[slot]);
    if (ret <= 0)
    {
        print_openssl_error(slot, ret);
        SSL_free(g_ssl[slot]);
        g_ssl[slot] = NULL;
        return -1;
    }

    return slot;
}


/* ==========================================================================
    Closes ssl connection, it does not close(2) systems socket.
   ========================================================================== */


int ssl_close
(
    int  ssl_fd  /* ssl fd as returned from ssl_accept() */
)
{
    SSL_free(g_ssl[ssl_fd]);

    /* set ssl slot to NULL, to mark it as free */

    g_ssl[ssl_fd] = NULL;

    return 0;
}


/* ==========================================================================
    Just like write(2) but data will be sent over encrypted layer.
   ========================================================================== */


ssize_t ssl_write
(
    int          ssl_fd,  /* ssl fd as returned from ssl_accept() */
    const void  *buf,     /* unencrypted data to send */
    size_t       count    /* number of bytes to send */
)
{
    int          ret;     /* return from SSL_write() */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, count > 0);

    ret = SSL_write(g_ssl[ssl_fd], buf, count);
    if (ret < 0)
    {
        print_openssl_error(ssl_fd, ret);
        return -1;
    }

    return (ssize_t)ret;
}


/* ==========================================================================
    Just like read(2) but for data received over encrypted layer.
   ========================================================================== */


ssize_t ssl_read
(
    int      ssl_fd,  /* ssl fd as returned from ssl_accept() */
    void    *buf,     /* unencrypted data to send */
    size_t   count    /* number of bytes to send */
)
{
    int          ret;     /* return from SSL_read() */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    ret = SSL_read(g_ssl[ssl_fd], buf, count);
    if (ret < 0)
    {
        print_openssl_error(ssl_fd, ret);
        return -1;
    }

    return (ssize_t)ret;
}


/* ==========================================================================
    Shuts down an active ssl connection.
   ========================================================================== */


int ssl_shutdown
(
    int  ssl_fd,  /* ssl fd as returned from ssl_accept() */
    int  how      /* bi- or uni-directional shutdown? */
)
{
    /* one call is for unidirectional shutdown */

    SSL_shutdown(g_ssl[ssl_fd]);

    if (how == SHUT_RDWR)
    {
        /* for bidirectional shutdown we need to call it twice */

        SSL_shutdown(g_ssl[ssl_fd]);
    }

    return 0;
}

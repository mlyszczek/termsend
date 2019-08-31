/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / This is server. Server serves services. Here we create      \
        | server socket, accept connections, check if ip is banned or |
        | not, and process upload of the data as well as storing it   |
        \ into file later                                             /
         -------------------------------------------------------------
           \                      .       .
            \                    / `.   .' "
             \           .---.  <    > <    >  .---.
              \          |    \  \ - ~ ~ - /  /    |
             _____          ..-~             ~-..-~
            |     |   \~~~\.'                    `./~~~/
           ---------   \__/                        \__/
          .'  O    \     /               /       \  "
         (_____,    `._.'               |         }  \/~~~/
          `----.          /       }     |        /    \__/
                `-.      |       /      |       /      `. ,~~|
                    ~-.__|      /_ - ~ ^|      /- _      `..-'
                         |     /        |     /     ~-.     `-. _  _  _
                         |_____|        |_____|         ~ - . _ _ _ _ _>
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "feature.h"

#include <arpa/inet.h>
#include <embedlog.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if HAVE_SYS_SELECT_H
    /* hpux doesn't have select.h file, maybe there are other
     * systems like this (?)
     */
#   include <sys/select.h>
#endif

#include "bnwlist.h"
#include "config.h"
#include "globals.h"
#include "server.h"
#include "ssl/ssl.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


/* default object when printing with el_o* functions */

#define EL_OPTIONS_OBJECT &g_qlog

/* struct holding info about server socket */

struct sinfo
{
    int  fd;     /* systems file descriptor of socket */
    int  ssl;    /* is this ssl connection? */
    int  sslfd;  /* if ssl is enabled, holds ssl fd for ssl_* functions */
    int  timed;  /* is this timed-enabled port? */
};

struct cinfo
{
    int              cfd;
    int              ffd;
    int              ssl;
    int              sslfd;
    int              timed;
    char             fname[32];
    struct timespec  timeout_at;
    size_t           written;
};

static struct sinfo  *si;   /* server info array for all interfaces */
static unsigned       nsi;  /* number of server info allocated */
static struct cinfo  *ci;   /* client info array of connected clients sockets */
static unsigned       nci;  /* number of client info allocated */


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    returns statically allocated string with IP related to fd
   ========================================================================== */


static const char *server_get_ips
(
    int                 fd      /* fd to get ip from */
)
{
    socklen_t           clen;   /* size of client address */
    struct sockaddr_in  client; /* client socket info */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    clen = sizeof(client);
    getpeername(fd, (struct sockaddr *)&client, &clen);
    return inet_ntoa(client.sin_addr);
}


/* ==========================================================================
    Returns number of busy slots. Busy slot means that client is connected
    and we are still processing it.
   ========================================================================== */


static unsigned server_num_busy_slot(void)
{
    unsigned  n;  /* number of busy slots */
    unsigned  i;  /* iterator */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (i = 0, n = 0; i != nci; ++i)
        if (ci[i].cfd >= 0)
            ++n;

    return n;
}


/* ==========================================================================
    Searches for available slot for client connection.


    returns
            >=0     index for client info with free slot
            -1      no free slots available
   ========================================================================== */


static int server_get_free_client(void)
{
    unsigned  i;  /* iterator */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (i = 0; i != nci; ++i)
        if (ci[i].cfd == -1)
            return i;

    return -1;
}


/* ==========================================================================
    returns number of ip in g_config.bind_ip list. List is a comma separated
    list of ips.
   ========================================================================== */


static int server_bind_num(void)
{
    int          n;    /* number of ip to bind to */
    const char  *ips;  /* comma separated list of ips to bind to */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (n = 1, ips = g_config.bind_ip; *ips; ++ips)
    {
        if (*ips == ',')
        {
            ++n;
        }
    }

    return n;
}


/* ==========================================================================
    Function generates random string of length l that is stored in buffer s.
    Caller is responsible for making s big enoug to hold l + 1 number of
    bytes. Returned string contains only numbers and lower-case characters.
    String will be null terminated.
   ========================================================================== */


static void server_generate_fname
(
    char                *s,
    size_t               l
)
{
    static const char    alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    size_t               i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (i = 0; i != l; ++i)
        *s++ = alphanum[rand() % (sizeof(alphanum) - 1)];

    *s = '\0';
}


/* ==========================================================================
    Function sends FIN to the client and waits for FIN from client. This is
    done so we can know when client received all of our messages we sent to
    him.
   ========================================================================== */


static void server_linger
(
    struct cinfo  *c           /* info about connected client */
)
{
    unsigned char  buf[8192];  /* dummy buffer to get data from read */
    ssize_t        r;          /* return value from read */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* inform client that writing any more data is not allowed */

    if (c->ssl)
        ssl_shutdown(c->sslfd, SHUT_RDWR);

    shutdown(c->cfd, SHUT_RDWR);

    for (;;)
    {
        r = c->ssl ? ssl_read(c->sslfd, buf, sizeof(buf)) :
            read(c->cfd, buf, sizeof(buf));

        if (r < 0)
        {
            /* some error occured, It doesn't matter why, we stop
             * lingering anyway.  Worst thing that can happen is
             * that client won't receive error message. We can live
             * with that
             */

            el_perror(ELW, "read error");
            return;
        }

        if (r == 0)
        {
            /* Client received our FIN, and sent back his FIN. Now
             * we can be fairly sure, client received all our
             * messages.
             */

            return;
        }

        /* we ignore any data received from the client - time for
         * talking is over.
         */
    }
}


/* ==========================================================================
    formats message pointer by fmt and sends it all to client associated
    with fd. In case of any error from write function, we just log situation
    but sending is interrupted and client won't receive whole message (if he
    receives anything at all)
   ========================================================================== */


static void server_reply
(
    struct cinfo  *fdi,        /* client to send message to */
    const char    *fmt,        /* message format (see printf(3)) */
                   ...         /* variadic arguments for fmt */
)
{
    size_t         written;    /* number of bytes written by write so far */
    size_t         mlen;       /* final size of the message to send */
    char           msg[1024];  /* message to send to the client */
    va_list        ap;         /* variadic argument list from '...' */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    va_start(ap, fmt);
    mlen = vsprintf(msg, fmt, ap);
    va_end(ap);

    /* send reply in loop until all bytes are commited to the
     * kernel for sending
     */

    written = 0;
    while (written != mlen)
    {
        ssize_t  w;  /* number of bytes written by write */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        w = fdi->ssl ? ssl_write(fdi->sslfd, msg + written, mlen - written) :
            write(fdi->cfd, msg + written, mlen - written);

        if (w == -1)
        {
            el_perror(ELE, "[%3d] error writing reply to the client", fdi->cfd);
            return;
        }

        written += w;
    }
}


/* ==========================================================================
    this function creates server socket that is fully configured and is
    ready to accept connections
   ========================================================================== */


static int server_create_socket
(
    in_addr_t           ip,     /* local ip to bind server to */
    unsigned            port    /* port to bind server to */
)
{
    int                 fd;     /* new server file descriptor */
    int                 flags;  /* flags for setting socket options */
    struct sockaddr_in  srv;    /* server address to bind to */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        el_perror(ELF, "couldn't create server socket");
        return -1;
    }

    /* as TCP is all about reliability, after server crashes (or is
     * restarted), kernel still keeps our server tuple in TIME_WAIT
     * state, to make sure all connections are closed properly
     * disallowing us to bind to that address again. We don't need
     * such behaviour, thus we allow SO_REUSEADDR
     */

    flags = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) != 0)
    {
        el_perror(ELF, "failed to set socket to SO_REUSEADDR");
        close(fd);
        return -1;
    }

    /* fill server address parameters for listening */

    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = ip;

    /* bind socket to srv address, so it only accept connections
     * from this ip/interface
     */

    if (bind(fd, (struct sockaddr *)&srv, sizeof(srv)) != 0)
    {
        el_perror(ELF, "failed to bind to socket");
        close(fd);
        return -1;
    }

    /* mark socket to accept incoming connections. Backlog is set
     * high enough so that no client can receive connection refused
     * error.
     */

    if (listen(fd, 256) != 0)
    {
        el_perror(ELF, "failed to make socket to listen");
        close(fd);
        return -1;
    }

    return fd;
}


/* ==========================================================================
    Checks when current client timeouts and arm timer to that value. Timer
    is set only when either it is not armed or requests 's' is smaller than
    current timer.
   ========================================================================== */


static void server_rearm_timer
(
    struct cinfo     *c     /* rearm timer based on this client */
)
{
    struct itimerval  it;   /* timer value */
    struct timespec   next; /* how long till next timeout for client c */
    struct timespec   now;  /* current time */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* how long till next timeout on 'c' should occur? */

    memset(&it, 0x00, sizeof(it));
    clock_gettime(CLOCK_MONOTONIC, &now);
    next.tv_sec = c->timeout_at.tv_sec - now.tv_sec;
    next.tv_nsec = c->timeout_at.tv_nsec - now.tv_nsec;

    if (next.tv_nsec < 0)
    {
        next.tv_sec -= 1;
        next.tv_nsec += 1000000000l;
    }

    if (next.tv_sec <= 0)
    {
        /* whoops, we are already past schedule, set timer to expire
         * as soon as possible
         */

        it.it_value.tv_sec  = 0;
        it.it_value.tv_usec = 1;
        setitimer(ITIMER_REAL, &it, NULL);
        el_print(ELD, "we are past schedule; set timer to 1usec");
        return;
    }

    getitimer(ITIMER_REAL, &it);
    el_print(ELD, "timer expire in: %ld.%03d, c expire in: %ld.%03d",
            (long)it.it_value.tv_sec, it.it_value.tv_usec / 1000l,
            (long)next.tv_sec, next.tv_nsec / 1000000l);

    if (it.it_value.tv_sec == 0 && it.it_value.tv_usec == 0)
    {
        /* timer is currently disarmed, arm it now and leave */

        it.it_value.tv_sec = next.tv_sec;
        it.it_value.tv_usec = next.tv_nsec / 1000l;
        setitimer(ITIMER_REAL, &it, NULL);
        el_print(ELD, "timer armed to expire in %ld.%03d",
                (long)next.tv_sec, next.tv_nsec / 1000000l);
        return;
    }

    /* if timer is currently armed and expires earlier then what
     * was requested, do not change timer
     */

    if ((it.it_value.tv_sec < next.tv_sec) ||
            (it.it_value.tv_sec == next.tv_sec &&
             it.it_value.tv_usec < next.tv_nsec / 1000l))
        return;

    /* our timer needs to expire before current one, so override */

    it.it_value.tv_sec  = next.tv_sec;
    it.it_value.tv_usec = next.tv_nsec / 1000;
    setitimer(ITIMER_REAL, &it, NULL);
    el_print(ELD, "timer armed to expire in %ld.%03d",
            (long)next.tv_sec, next.tv_nsec / 1000000l);
}


/* ==========================================================================
    This is heart of the swarm... erm I mean of the server. This function is
    a threaded function, it is fired up everytime client connects and passes
    checks for maximum connection, black and white list etc. Function handle
    upload from the client and storing it in the file, it also takes care of
    network problems and end string detection. Here, if socket is ssl or tls
    enabled, cfd->sslfd is after successfull ssl handshake.
   ========================================================================== */


static void server_process_client
(
    struct cinfo       *c        /* current client to process */
)
{
    struct timespec     now;         /* current time */
    char                url[8192 + 1];   /* generated link to uploaded data */
    char                ends[9 + 1]; /* buffer for end string detection */
    unsigned char       buf[8192];   /* temp buffer we read uploaded data to */
    ssize_t             w;           /* return from write function */
    ssize_t             r;           /* return from read function */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    clock_gettime(CLOCK_MONOTONIC, &now);
    el_print(ELD, "processing client %3d", c->cfd);

    if (g_sigalrm)
    {
        /* SIGALRM has been received, did our client timedout? */

        el_print(ELD, "handling SIGALRM, now: %lld.%03d, timeout at: %lld.%03d",
                (long long)now.tv_sec, now.tv_nsec / 1000000l,
                (long long)c->timeout_at.tv_sec,
                c->timeout_at.tv_nsec / 1000000l);

        if ((now.tv_sec < c->timeout_at.tv_sec) ||
                (now.tv_sec == c->timeout_at.tv_sec &&
                 now.tv_nsec < c->timeout_at.tv_nsec))
        {
            /* no, that wasn't us, rearm timer unless someone else
             * is going to timeout before us
             */

            server_rearm_timer(c);
            return;
        }

        /* yes, what should we do with it? */

        if (c->timed)
        {
            /* time upload was enabled, in that case we don't treat
             * timeout as error but we assume user has no more data
             * to send and we should store it and send him the link
             * to data he just uploaded.
             */

            goto upload_finished_with_timeout;
        }
        else
        {
            /* no activity from client for max_timeout seconds,
             * either client died and didn't tell us about it
             * (thanks!) or connection was abrupted by some higher
             * forces. We assume this is unrecoverable problem and
             * close connection
             */

            el_print(ELN, "[%3d] client inactive for %d seconds",
                    c->cfd, g_config.max_timeout);
            el_oprint(OELI, "[%s] rejected: inactivity",
                    server_get_ips(c->cfd));

            /* well, there may be one more case for inactivity from
             * clients side. It may be that he forgot to add ending
             * string "termsend\n", so we send reply to the client
             * as there is a chance he is still alive.
             */

            server_reply(c, "disconnected due to inactivity for %d "
                "seconds, did you forget to append termination "
                "string - \"termsend\\n\"?\n", g_config.max_timeout);
            goto error;
        }
    }

    /* no SIGALRM means there is data to be read from client.
     * read() won't block since we've checked it with select()
     */

    r = c->ssl ? ssl_read(c->sslfd, buf, sizeof(buf)) :
        read(c->cfd, buf, sizeof(buf));

    if (r == -1)
    {
        /* error from read, and we know it cannot be EAGAIN as
         * select covered that for us, so something wrong must
         * have happened.  Inform client and close connection.
         */

        el_perror(ELC, "[%3d] couldn't read from client", c->cfd);
        el_oprint(OELI, "[%s] rejected: read error", server_get_ips(c->cfd));
        server_reply(c, "internal server error, try again later\n");
        goto error;
    }

    /* r == 0 means that client gently closed connection by sending
     * FIN, and nicely waits for us to respond, in that case we do
     * not require client to send ending termsend\n
     */

    if (r == 0)
        goto upload_finished_with_fin;

    if (c->written + r > (size_t)g_config.max_size + 9)
    {
        /* we received, in total, more bytes then we can accept, we
         * remove such file and return error to the client. That +9
         * is for ending string "termsend\n" as we will delete that
         * anyway and file will not get more than g_config.max_size
         * size.
         */

        el_oprint(OELI, "[%s] rejected: file too big", server_get_ips(c->cfd));
        server_reply(c, "file too big, max length is %ld bytes\n",
            g_config.max_size);
        goto error;
    }

    /* received some data, simply store them into file, right now
     * we don't care if ending string "termsend\n" ends up in a
     * file, we will take care of it later.
     */

    if ((w = write(c->ffd, buf, r)) != r)
    {
        el_perror(ELC, "[%3d] couldn't write to file", c->cfd);
        el_oprint(OELI, "[%s] rejected: write to file failed",
                server_get_ips(c->cfd));
        server_reply(c, "internal server error, try again later\n");
        goto error;
    }

    /* write was successful, now let's check if data written to
     * file contains ending string "termsend\n". For that we read 9
     * last characters from data stored in file and if there are
     * not enough bytes in the file yet, simply do nothing and wait
     * for more data
     */

    if ((c->written += w) < 9)
    {
        server_rearm_timer(c);
        c->timeout_at.tv_sec = now.tv_sec +
                (c->timed ? g_config.timed_max_timeout : g_config.max_timeout);
        c->timeout_at.tv_nsec = now.tv_nsec;
        el_print(ELD, "got data, now: %lld.%03d, next timeout at: %lld.%03d",
                (long long)now.tv_sec, now.tv_nsec / 1000000l,
                (long long)c->timeout_at.tv_sec,
                c->timeout_at.tv_nsec / 1000000l);

        return;
    }

    /* we seek 9 bytes back, as its length of "termsend\n" and read
     * last 9 bytes to check for end string existance. We don't
     * need to seek back to end of file, as reading will move the
     * pointer by itself.
     */

    lseek(c->ffd, -9, SEEK_CUR);

    if (read(c->ffd, ends, 9) != 9)
    {
        /* totally unexpected, but still we expect it, like a good
         * swat team. We don't know how to recover from this error,
         * so let's call it a day for this client
         */

        el_perror(ELC, "[%3d] couldn't read end string", c->cfd);
        el_oprint(OELI, "[%s] rejected: end string read error",
                server_get_ips(c->cfd));
        server_reply(c, "internal server error, try again later\n");
        goto error;
    }

    ends[sizeof(ends) - 1] = '\0';
    if (strcmp(ends, "termsend\n") != 0)
    {
        /* ending string has not yet been received, we continue
         * getting data from client. Reset timeout timer.
         */

        server_rearm_timer(c);

        /* since we have received some data it means client is
         * active, reset timeout timer
         */

        c->timeout_at.tv_sec = now.tv_sec +
                (c->timed ? g_config.timed_max_timeout : g_config.max_timeout);
        c->timeout_at.tv_nsec = now.tv_nsec;
        el_print(ELD, "got data, now: %lld.%03d, next timeout at: %lld.%03d",
                (long long)now.tv_sec, now.tv_nsec / 1000000l,
                (long long)c->timeout_at.tv_sec,
                c->timeout_at.tv_nsec / 1000000l);

        return;
    }

    /* full file received without errors, we even got ending
     * string, we can now break out of loop to perform finishing
     * touch on upload.
     *
     * to finish, we need to truncate file, to cut off ending
     * string from file and close it. We carefully check in
     * previous lines that written is at least 9 bytes long, so
     * this subtact is ok.
     */

    c->written -= 9;
    if (ftruncate(c->ffd, c->written) != 0)
    {
        el_perror(ELC, "[%3d] couldn't truncate file from ending string",
                c->cfd);
        el_oprint(OELI, "[%s] rejected: truncate failed",
                server_get_ips(c->cfd));
        server_reply(c, "internal server error, try again later\n");
        goto error;
    }

    /* we will jump to this only when timed upload is enabled and
     * connection has timed out. In that case we don't need (we
     * cannot!) truncate dta by 9 bytes of "termsend\n" string,
     * because such string was not (and could not have been) sent
     * to us.
     */

upload_finished_with_timeout:

    /* another scenario is when client sends all data and gently
     * closes connection - and thus nicely waits for us to respond
     * with link. Treat is like finish with timeout.
     */

upload_finished_with_fin:

    /* one more thing to check after upload finishes, that is if
     * there was any real data transmited, if not, emit error and
     * not save empty file
     */

    if (c->written == 0)
    {
        el_oprint(OELI, "[%s] rejected: no data has been sent",
                server_get_ips(c->cfd));
        server_reply(c, "no data has been sent\n");
        goto error;
    }

    close(c->ffd);

    /* after upload is finished, we send the client, link where he
     * can download his newly uploaded file
     */

    strcpy(url, g_config.domain);
    strcat(url, "/");
    strcat(url, c->fname);
    el_oprint(OELI, "[%s] %s", server_get_ips(c->cfd), c->fname);
    server_reply(c, "%s\n", url);
    server_linger(c);
    if (c->ssl) ssl_close(c->sslfd);
    close(c->cfd);
    c->cfd = -1;

    return;

error:
    /* this handles any error during file reception, we remove
     * unfinished upload and close client's connection
     */

    server_linger(c);
    if (c->ssl) ssl_close(c->sslfd);
    close(c->cfd);
    close(c->ffd);
    c->cfd = -1;
    unlink(c->fname);
}


/* ==========================================================================
    Initializes client so it cat start transfering data. This is done only
    once per client right after connection.

    return
            0       client initialized with success
           -1       critical error, client not initialized, connection
                    has been dropped.
   ========================================================================== */


static int server_init_client
(
    struct cinfo    *cfd          /* info about client */
)
{
    int              ncollision;  /* number of file name collisions hit */
    struct timespec  now;         /* current time */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* first, let's generate unique name to reference data client is
     * sending
     */

    ncollision = 0;
    for (;;)
    {
        static unsigned  flen = 5; /* length of the filename to generate */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        if (flen >= sizeof(cfd->fname))
        {
            static unsigned  warnings_emitted;  /* number of warnings printed */
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            /* somehow we hit maximum filename, this is like, super
             * unlikely, but of course may happen and when it does,
             * limit size of flen to not overflow cfd->fname. It is
             * possible we will hang server while it searches for
             * free file, but it's better than buffer overflow
             */

            flen = sizeof(cfd->fname) - 1;
            ++warnings_emitted;

            if (warnings_emitted <= 10)
                el_print(ELW, "flen reached its maximum value of %u", flen);

            /* in case we hit colision everytime, and everytime we
             * overflow flen, we will spam log file with flen warnings
             * forever, which may lead to log file getting fat rapidly,
             * user has been warned 10 times about the situation, this
             * is enough, no more spam (until we overflow variable).
             */

            if (warnings_emitted == 10)
                el_print(ELW, "flen warning happens too often, no more");
        }

        server_generate_fname(cfd->fname, flen);

        cfd->ffd = open(cfd->fname, O_CREAT | O_EXCL | O_APPEND | O_RDWR, 0644);

        /* if file has opened with success, break out of the loop */

        if (cfd->ffd >= 0)
            break;

        if (errno == EEXIST)
        {
            /* we hit file name collision, increment collision
             * counter, and if that counter is bigger than 3, we
             * increment file length by one, because it looks like
             * there are a lot of files with current file length
             */

            if (++ncollision == 3)
            {
                ++flen;
                ncollision = 0;
                el_print(ELN, "increasing flen by one to: %u", flen);
            }

            continue;
        }

        /* unexpected error occured, log situation and close
         * connection
         */

        el_perror(ELA, "[%3d] couldn't open file %s/%s", cfd->cfd,
                g_config.output_dir, cfd->fname);
        el_oprint(OELI, "[%s] rejected: file open error",
                server_get_ips(cfd->cfd));
        server_reply(cfd, "internal server error, try again later\n");
        if (cfd->ssl) ssl_close(cfd->sslfd);
        close(cfd->cfd);
        cfd->cfd = -1;
        return -1;
    }

    cfd->written = 0;
    clock_gettime(CLOCK_MONOTONIC, &now);
    cfd->timeout_at.tv_sec = now.tv_sec +
            (cfd->timed ? g_config.timed_max_timeout : g_config.max_timeout);
    cfd->timeout_at.tv_nsec = now.tv_nsec;

    /* if client connects but does not send anything, select() never
     * returns and we could have ghost connection that occupies slot
     * (dos attack) unless another connection triggers SIGALRM. To
     * counter it, we arm timer right here even before we receive
     * any byte
     */

    server_rearm_timer(cfd);
    return 0;
}

/* ==========================================================================
    in this function we accept connection from the backlog queue, check if
    client is allowed to upload and if server has free upload slots. If all
    checks pass, function starts thread that will handle upload on its own.
   ========================================================================== */


static void server_process_connection
(
    struct sinfo       *sfd      /* server socket we accept connection from */
)
{
    int                 acfd;    /* fd for accepted client connection */
    int                 slot;    /* free slot for client */
    socklen_t           clen;    /* length of 'client' variable */
    struct cinfo       *cfd;     /* current client information */
    struct sockaddr_in  client;  /* address of remote client */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    clen = sizeof(client);
    el_print(ELD, "processing new connection");

    /* accept incoming connection, it will be instant because we
     * checked with select() ther client is awaiting.
     */

    if ((acfd = accept(sfd->fd, (struct sockaddr *)&client, &clen)) < 0)
    {
        el_perror(ELC, "couldn't accept connection");
        el_oprint(OELI, "[NULL] rejected: accept error");
        return;
    }

    el_print(ELI, "incoming %sssl connection from %s socket id %d",
        sfd->ssl ? "" : "non-", inet_ntoa(client.sin_addr), acfd);

    /* server is going down, do not accept any new connections */

    if (g_shutdown)
        close(acfd);

    /* get free upload slot for client, of no slot is available,
     * that means connection limit is reached.
     */

    slot = server_get_free_client();
    if (slot == -1)
    {
        struct cinfo  cfd;  /* temp cinfo object for server_reply() */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        /* since we couldn't get free slot for the client, but
         * we still want to reply to the user, and to the fact
         * that server_reply() accepts struct cinfo, we
         * create temporary cinfo, for that purpose. We only
         * have to set fields needed by server_reply().
         */

        cfd.cfd = acfd;
        cfd.ssl = 0;

        el_oprint(OELI, "[%s] rejected: connection limit",
            inet_ntoa(client.sin_addr));
        server_reply(&cfd, "all upload slots are taken, try again later\n");
        close(acfd);
        return;
    }

    cfd = &ci[slot];
    cfd->cfd = acfd;

    /* at this point, we still have normal unencrypted connection,
     * so set ssl to 0, so that server_reply() sends possible error
     * data (without any sensitive informations) over non-ssl
     * socket.
     */

    cfd->ssl = 0;

    /* after accepting connection, we have client's ip, now we
     * check if this ip can upload (it can be banned, or not
     * listed in the whitelist, depending on server config.
     */

    if (bnw_is_allowed(ntohl(client.sin_addr.s_addr)) == 0)
    {
        el_oprint(OELI, "[%s] rejected: not allowed",
            inet_ntoa(client.sin_addr));
        server_reply(cfd, "you are not allowed to upload to this server\n");
        close(cfd->cfd);
        cfd->cfd = -1;
        return;
    }

    /* perform ssl handshake */

    if (sfd->ssl)
    {
        cfd->sslfd = ssl_accept(cfd->cfd);
        if (cfd->sslfd == -1)
        {
            el_oprint(OELI, "[%s] rejected: ssl_accept() error",
                inet_ntoa(client.sin_addr));

            /* ssl negotation failed, reply in clear text */

            server_reply(cfd, "kurload: ssl negotation failed\n");
            close(cfd->cfd);
            cfd->cfd = -1;
            return;
        }

        /* now connection is encrypted, mark that in clients
         * socket info
         */

        cfd->ssl = 1;
    }

    /* copy information if client should perform timed uploads
     * or not
     */

    cfd->timed = sfd->timed;

    /* client is connected, allowed and connection limit has
     * not been reached, now initialize client's state struct
     * so it can start transfering data.
     */

    server_init_client(cfd);
}


/* ==========================================================================
    Functions creates sockets for port for each listen ip (interface)
    specified in config
   ========================================================================== */


static int create_socket_for_ips
(
    unsigned     port,       /* port to create sockets for */
    int          timed,      /* is this timed-enabled upload port? */
    int          ssl,        /* is this ssl port? */
    unsigned     nips,       /* number of ips to listen on*/
    unsigned    *port_index  /* port index being parsed */
)
{
    unsigned     i;          /* current server info array index */
    char         bip[sizeof(g_config.bind_ip)];  /* copy of g_config.bind_ip */
    const char  *ip;         /* tokenized ip from bip */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if (port <= 0)
    {
        /* port is no used, ignore */

        return 0;
    }

    strcpy(bip, g_config.bind_ip);
    ip = strtok(bip, ",");

    for (i = *port_index * nips; i != *port_index * nips + nips; ++i)
    {
        in_addr_t    netip;  /* ip of the interface to listen on */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        netip = inet_addr(ip);

        el_print(ELN, "creating server %s:%d (%s, %s)", ip, port,
            timed ? "    timed" : "not timed", ssl ? "    ssl" : "non-ssl");
        if ((si[i].fd = server_create_socket(netip, port)) < 0)
        {
            el_print(ELF, "couldn't create socket for %s:%d", ip, port);
            return -1;
        }

        si[i].ssl = ssl;
        si[i].timed = timed;

        /* get next ip address on the list */

        ip = strtok(NULL, ",");
    }

    /* increase port index, so next time we write to proper server
     * info array index
     */

    *port_index += 1;
    return 0;
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
    this creates server (or servers if we will listen of multiple
    interfaces) to handle connections from the clients. It initializes all
    memory, structures to valid state. If function returns 0, you're all set
    and you can call loop_forever(), else server is in invalid state and
    should not run
   ========================================================================== */


int server_init(void)
{
    int       e;       /* error from function */
    unsigned  i;       /* simple iterator */
    unsigned  pi;      /* current port index */
    unsigned  nports;  /* number of listen ports */
    unsigned  nips;    /* number of ips to listen on*/
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    el_print(ELN, "creating server");

    /* calculate how many listen ports do we have */

    nports = 0;
    nports = g_config.listen_port > 0           ? nports + 1 : nports;
    nports = g_config.ssl_listen_port > 0       ? nports + 1 : nports;
    nports = g_config.timed_listen_port > 0     ? nports + 1 : nports;
    nports = g_config.timed_ssl_listen_port > 0 ? nports + 1 : nports;

    /* number of server sockets to open, this is number of
     * ips we are going to listen on times number of ports
     * we will be listening on. So each ip will listen on
     * each port
     */

    nips = server_bind_num();
    nsi = nports * nips;
    nci = g_config.max_connections;

    /* allocate memory for all server sockets, one interface equals
     * one server socket.
     */

    if ((si = malloc(nsi * sizeof(*si))) == NULL)
    {
        el_print(ELF, "couldn't allocate memory for %d server(s)", nsi);
        return -1;
    }

    /* allocate memory for all client sockets, one socket for each
     * connection
     */

    if ((ci = malloc(nci * sizeof(*ci))) == NULL)
    {
        el_print(ELF, "couldn't allocate memory for %u client(s)", nci);
        free(si);
        return -1;
    }

    /* invalidate all allocated server and client sockets, so
     * closing such socket in case of an error won't crash the app.
     */

    for (i = 0; i != nsi; ++i)
        si[i].fd = -1;

    for (i = 0; i != nci; ++i)
        ci[i].cfd = -1;

    /* Now we create one server socket for each interface:port user
     * specified in configuration file.
     */

    pi = 0;
    e = 0;
    e |= create_socket_for_ips(g_config.listen_port, 0, 0, nips, &pi);
    e |= create_socket_for_ips(g_config.ssl_listen_port, 0, 1, nips, &pi);
    e |= create_socket_for_ips(g_config.timed_listen_port, 1, 0, nips, &pi);
    e |= create_socket_for_ips(g_config.timed_ssl_listen_port, 1, 1, nips, &pi);

    if (e) goto error;

    /* seed random number generator for generating unique file name
     * for uploaded files. We don't need any cryptographic
     * security, so simple random seeded with current time is more
     * than enough for us.
     */

    srand(time(NULL));

    /* if ssl port is enabled, initialize ssl */

    if (g_config.ssl_listen_port || g_config.timed_ssl_listen_port)
        ssl_init();

    /* change dir to the output directory, so we can open suffix
     * directly without passing full path, like: open("f3jds", ...)
     * instaed of open("/var/lib/termsend/f3jds", ...), which saves
     * us from constructing path for open() each time we generate
     * filename
     */

    if (chdir(g_config.output_dir) != 0)
    {
        el_perror(ELF, "chdir(%s)", g_config.output_dir);
        goto error;
    }

    return 0;

error:
    server_destroy();
    return -1;
}


/* ==========================================================================
    main server loop, here we await connections and process them
   ========================================================================== */


void server_loop_forever(void)
{
    fd_set    readfds;     /* set containing all server sockets to monitor */
    time_t    prev_flush;  /* time when flush was last called */
    int       maxfd;       /* maximum fd value monitored in readfds */
    sigset_t  sigblk;      /* signals to block */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    sigemptyset(&sigblk);
    sigaddset(&sigblk, SIGALRM);

    prev_flush = 0;
    el_print(ELN, "server initialized and started");

    for (;;)
    {
        int       sact;     /* select activity, just select return value */
        unsigned  i;        /* a simple interator for loop */
        time_t    now;      /* current time from time() */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        now = time(NULL);
        if ((now - prev_flush) >= 60)
        {
            /* flush logs everytime something happens, but not
             * more frequent than once per minute
             */

            el_flush();
            prev_flush = now;
        }

        /* we may have multiple server sockets, so we cannot accept
         * in blocking fassion. Since number of server sockets will
         * be very small, we can use not so fast but highly
         * portable select. Let's prepare our fdset. This must be
         * done in the loop each time, select() is about to be
         * called, since select() modified fs_sets.
         */

        FD_ZERO(&readfds);

        for (i = 0, maxfd = 0; i != nsi; ++i)
        {
            /* we validate all sockets in init function, so we are
             * sure server info array is valid and contains valid
             * file descriptors unless user called this function
             * without init or when init failed and return code
             * wasn't checked, then he deserves nice segfault in da
             * face
             */

            FD_SET(si[i].fd, &readfds);

            /* we need to find which socket is the highest one,
             * select needs this information to process fds without
             * segfaults
             */

            maxfd = si[i].fd > maxfd ? si[i].fd : maxfd;
        }

        for (i = 0; i != nci; ++i)
        {
            if (ci[i].cfd == -1)
                continue;

            FD_SET(ci[i].cfd, &readfds);
            maxfd = ci[i].cfd > maxfd ? ci[i].cfd : maxfd;
        }

        /* double SIGTERM received, we need to exit RIGHT NOW,
         * so no more client processsing
         */

        if (g_stfu)
            return;

        /* now we wait for activity, for server sockets activity
         * means we have an incoming connection. For client
         * sockets, it means we have outstanding data to read. We
         * pass NULL for timeout, because we want to wait
         * indefinitely, and we are not interested in writefds (we
         * don't write to any socket) nor exceptfds as exceptions
         * don't occur on server sockets.
         *
         * We use SIGALRM to indicate that any of the client socket
         * has timed out and action must be taken. We don't want
         * SIGALRM to interrupt any of read()/write() call during
         * client processing code, so we block that signal after
         * select() and unblock it before select(). It's ok for
         * select to be interrupted by SIGALRM.
         */

        sigprocmask(SIG_UNBLOCK, &sigblk, NULL);

        /* call select() only when SIGALRM has not been received,
         * if it has, we process all clients immediately to check
         * which one has timedout
         */

        sact = -1;
        if (g_sigalrm == 0)
            sact = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        sigprocmask(SIG_BLOCK, &sigblk, NULL);

        if (sact == -1 && g_sigalrm == 0 && g_shutdown == 0)
        {
            /* if select has been interrupted by something we didn't
             * expect (and we expect SIGALRM and SIGTERM for shutdown)
             * then it means critical error and we interrupt program,
             * since there is no clean way to avoid UB at this point.
             */

            el_perror(ELF, "error waiting on socket activity");
            return;
        }

        /* if we get here, that means activity is on any server
         * socket, since we wait indefinietly, select cannot return
         * 0. Now we check which socket got activity and we accept
         * connection to process it. Processing of server socket
         * makes sense only when there is actual action there, so
         * if SIGALRM has been received (or select() simply did not
         * succeed), we do not process server sockets.
         */

        if (sact > 0)
            for (i = 0; i != nsi; ++i)
                if (FD_ISSET(si[i].fd, &readfds))
                    server_process_connection(&si[i]);

        /* now let's check if which (if any) client sent us some
         * data, it could also be that some client has timed out
         * but we don't know which one did that, so we have to
         * process all connected clients regardless of socket
         * activity to know that.
         */

        for (i = 0; i != nci; ++i)
        {
            if (ci[i].cfd == -1)
                continue;

            if (g_sigalrm || (sact > 0 && FD_ISSET(ci[i].cfd, &readfds)))
                server_process_client(&ci[i]);
        }

        /* SIGALRM has been handled (if there was any) */

        g_sigalrm = 0;

        /* if shutdown is not set, we continue flow of program */

        if (g_shutdown == 0)
            continue;

        /* server is going down but there still might be some
         * outstanding connections which we need to process.
         * Check if all connections are done and return from
         * loop only when all clients are processed. Busy slot
         * means client is connected there and is being processed.
         */

        if (server_num_busy_slot() == 0)
            return;

        /* double SIGTERM received, we exit without waiting for
         * clients to finish
         */

        if (g_stfu)
            return;
    }
}


/* ==========================================================================
    Waits for all connection to finish (unless double SIGTERM has been
    received) and then free resources that has been allocated.
   ========================================================================== */


void server_destroy(void)
{
    unsigned         i;    /* simple iterator for loop */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* close all server sockets, so any new connection is
     * automatically droped by the system
     */

    for (i = 0; i != nsi; ++i)
        close(si[i].fd);

    free(si);

    /* also close all outstanding connections */

    for (i = 0; i != nci; ++i)
    {
        if (ci[i].cfd == -1)
            continue;

        close(ci[i].cfd);

        /* close and remove any incomplete download */

        close(ci[i].ffd);
        unlink(ci[i].fname);
    }

    /* if ssl port enabled, cleanup ssl */

    if (g_config.ssl_listen_port)
        ssl_cleanup();
}

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
#include <pthread.h>
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

/* struct holding info about fd and whether is it ssl socket
 * or not
 */

struct fdinfo
{
    int  fd;      /* systems file descriptor of socket */
    int  ssl;     /* is this ssl connection? */
    int  ssl_fd;  /* if ssl is enabled, holds ssl fd for ssl_* functions */
    int  timed;   /* is this timed-enabled port? */
};

static struct fdinfo   *sfds;   /* sfds sockets for all interfaces */
static int              nsfds;  /* number of sfds allocated */
static int              cconn;  /* curently connected clients */
static pthread_mutex_t  lconn;  /* mutex lock for operation on cconn */
static pthread_mutex_t  lopen;  /* mutex for opening file */


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


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
    {
        *s++ = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    *s = '\0';
}


/* ==========================================================================
    Function sends FIN to the client and waits for FIN from client. This is
    done so we can know when client received all of our messages we sent to
    him.
   ========================================================================== */


static void server_linger
(
    struct fdinfo *fdi         /* connected clients file descriptor */
)
{
    unsigned char  buf[8192];  /* dummy buffer to get data from read */
    ssize_t        r;          /* return value from read */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* inform client that writing any more data is not allowed */

    if (fdi->ssl)
    {
        ssl_shutdown(fdi->ssl_fd, SHUT_RDWR);
    }

    shutdown(fdi->fd, SHUT_RDWR);

    for (;;)
    {
        r = fdi->ssl ? ssl_read(fdi->ssl_fd, buf, sizeof(buf)) :
            read(fdi->fd, buf, sizeof(buf));

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
    struct fdinfo *fdi,        /* client to send message to */
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


        w = fdi->ssl ? ssl_write(fdi->ssl_fd, msg + written, mlen - written) :
            write(fdi->fd, msg + written, mlen - written);

        if (w == -1)
        {
            el_perror(ELE, "[%3d] error writing reply to the client", fdi->fd);
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

    /* set server socket to work in non blocking manner, all
     * clients connecting to that socket will also inherit non
     * block nature
     */

    if ((flags = fcntl(fd, F_GETFL)) == -1)
    {
        el_perror(ELF, "error reading socket flags");
        close(fd);
        return INADDR_NONE;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        el_perror(ELF, "error setting socket to O_NONBLOCK");
        close(fd);
        return INADDR_NONE;
    }

    return fd;
}


/* ==========================================================================
    This is heart of the swarm... erm I mean of the server. This function is
    a threaded function, it is fired up everytime client connects and passes
    checks for maximum connection, black and white list etc. Function handle
    upload from the client and storing it in the file, it also takes care of
    network problems and end string detection. Here, if socket is ssl or tls
    enabled, cfd->ssl_fd is after successfull ssl handshake.
   ========================================================================== */


static void *server_handle_upload
(
    void               *arg          /* socket associated with client */
)
{
    struct sockaddr_in  client;      /* address of connected client */
    time_t              last_notif;  /* when last notif was sent */
    time_t              now;         /* current time */
    socklen_t           clen;        /* size of client address */
    fd_set              readfds;     /* set with client socket for select */
    struct timeval      cfdtimeo;    /* read timeout of clients socket */
    sigset_t            set;         /* signals to mask in thread */
    struct fdinfo      *cfd;         /* socket associated with client */
    int                 fd;          /* file where data will be stored */
    int                 ncollision;  /* number of file name collisions hit */
    int                 opathlen;    /* length of output directory path */
    char                path[PATH_MAX];  /* full path to the file */
    char                fname[32];   /* random generated file name */
    char                url[8192 + 1];   /* generated link to uploaded data */
    char                ends[8 + 1]; /* buffer for end string detection */
    static int          flen = 5;    /* length of the filename to generate */
    unsigned char       buf[8192];   /* temp buffer we read uploaded data to */
    size_t              written;     /* total written bytes to file */
    ssize_t             w;           /* return from write function */
    ssize_t             r;           /* return from read function */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    last_notif = time(NULL);
    cfd = arg;
    clen = sizeof(client);
    getpeername(cfd->fd, (struct sockaddr *)&client, &clen);

    strcpy(path, g_config.output_dir);
    strcat(path, "/");
    opathlen = strlen(path);

    /* we don't want threads that handle client connection to
     * receive signals, and thus interrupt system calls like write
     * or read. Only main thread can handle signals, so we mask all
     * signals here
     */

    sigfillset(&set);

    if (pthread_sigmask(SIG_SETMASK, &set, NULL) != 0)
    {
        el_perror(ELA, "couldn't mask signals");
        el_oprint(OELI, "[%s] rejected: signal mask failed",
                inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        if (cfd->ssl) ssl_close(cfd->ssl_fd);
        close(cfd->fd);
        free(cfd);
        return NULL;
    }

    /* generate unique file name for content that client will be
     * sending we start from file length = 5, and will increase
     * this value if we hit file collision 3 times in a row. flen
     * is static, because if we hit file collision once, there is
     * big enough chance we will hit it again, so to not waste cpu
     * power for checking it every single time client connects, we
     * keep the state in running serwer. When server gets
     * restarted, flen will be reseted back to default value, and
     * increment will again begin from low value. To avoid
     * situation, where 2 threads modify flen (leading to
     * incrementing flen by 2 instead of 1), only one thread can
     * open file at a time. Opening is fast, so it's not a big deal
     */

    pthread_mutex_lock(&lopen);
    ncollision = 0;
    for (;;)
    {
        server_generate_fname(fname, flen);
        strcpy(path + opathlen, fname);

        if ((fd = open(path, O_CREAT | O_EXCL | O_APPEND | O_RDWR, 0640)) >= 0)
        {
            /* file opened with success, break out of the loop */

            break;
        }

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
            }

            continue;
        }

        /* unexpected error occured, log situation and close
         * connection
         */

        pthread_mutex_unlock(&lopen);
        el_perror(ELA, "[%3d] couldn't open file %s", cfd->fd, path);
        el_oprint(OELI, "[%s] rejected: file open error",
                inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        if (cfd->ssl) ssl_close(cfd->ssl_fd);
        close(cfd->fd);
        free(cfd);
        pthread_mutex_lock(&lconn);
        --cconn;
        pthread_mutex_unlock(&lconn);
        return NULL;
    }
    pthread_mutex_unlock(&lopen);

    /* file is opened, we can now start to retrieve data from
     * client. Since purpose of this file server is that user can
     * upload data using only simple tools like netcat, we don't
     * have information about file size.  On top of that, we want
     * to send link to upload file, once upload is completed, so
     * netcat cannot close connection on upload complete, to inform
     * us about upload completed, so we expect an ending string at
     * the end of transfer that will inform us that transfer is
     * completed.  That ending string is "kurload\n". Yes, this may
     * lead to prepature end of transfer, but chances are so slim,
     * we can neglect them.
     */

    written = 0;
    FD_ZERO(&readfds);
    FD_SET(cfd->fd, &readfds);
    memset(ends, 0, sizeof(ends));

    for (;;)
    {
        int     sact;  /* select activity, just return from select() */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        cfdtimeo.tv_sec =
            cfd->timed ? g_config.timed_max_timeout : g_config.max_timeout;
        cfdtimeo.tv_usec = 0;

        now = time(NULL);
        sact = select(cfd->fd + 1, &readfds, NULL, NULL, &cfdtimeo);

        if (sact == -1)
        {
            /* select stumbled upon some error, it's a sad day for
             * our client, we need to interrupt connection
             */

            el_perror(ELW, "[%3d] select error on client read", cfd->fd);
            el_oprint(OELI, "[%s] rejected: select error",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        if (sact == 0)
        {
            if (cfd->timed)
            {
                /* time upload was enabled, in that case we don't
                 * treat timeout as error but we assume user has no
                 * more data to send and we should store it and
                 * send him the link to data he just uploaded.
                 */

                goto upload_finished_with_timeout;
            }

            /* no activity on cfd for max_timeout seconds, either
             * client died and didn't tell us about it (thanks!) or
             * connection was abrupted by some higher forces. We
             * assume this is unrecoverable problem and close
             * connection
             */

            el_print(ELN, "[%3d] client inactive for %d seconds",
                cfd->fd, g_config.max_timeout);
            el_oprint(OELI, "[%s] rejected: inactivity",
                inet_ntoa(client.sin_addr));

            /* well, there may be one more case for inactivity from
             * clients side. It may be that he forgot to add ending
             * string "kurload\n", so we send reply to the client
             * as there is a chance he is still alive.
             */

            server_reply(cfd, "disconnected due to inactivity for %d "
                "seconds, did you forget to append termination "
                "string - \"kurload\\n\"?\n", g_config.max_timeout);
            goto error;
        }

        /* and finnaly, we get here, when there is some data in
         * cfd, and we can safely call read, without fear of
         * locking
         */

        r = cfd->ssl ? ssl_read(cfd->ssl_fd, buf, sizeof(buf)) :
            read(cfd->fd, buf, sizeof(buf));

        if (r == -1)
        {
            /* error from read, and we know it cannot be EAGAIN as
             * select covered that for us, so something wrong must
             * have happened.  Inform client and close connection.
             */

            el_perror(ELC, "[%3d] couldn't read from client", cfd->fd);
            el_oprint(OELI, "[%s] rejected: read error",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        if (r == 0)
        {
            /* r == 0 means that client gently closes connection
             * by sending FIN, and nicely waits for us to respond,
             * in that case we do not require client to send ending
             * kurload\n
             */

            goto upload_finished_with_fin;
        }

        if (written + r > (size_t)g_config.max_size + 8)
        {
            /* we received, in total, more bytes then we can
             * accept, we remove such file and return error to the
             * client. That +8 is for ending string "kurload\n" as
             * we will delete that anyway and file will not get
             * more than g_config.max_size size.
             */

            el_oprint(OELI, "[%s] rejected: file too big",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "file too big, max length is %ld bytes\n",
                g_config.max_size);
            goto error;
        }

        /* received some data, simply store them into file, right
         * now we don't care if ending string "kurload\n" ends up
         * in a file, we will take care of it later.
         */

        if ((w = write(fd, buf, r)) != r)
        {
            el_perror(ELC, "[%3d] couldn't write to file", cfd->fd);
            el_oprint(OELI, "[%s] rejected: write to file failed",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        /* write was successful, now let's check if data written to
         * file contains ending string "kurload\n". For that we
         * read 8 last characters from data stored in file.
         */

        if ((written += w) < 8)
        {
            /* we didn't receive enough bytes to check for ending
             * string, so we don't check for ending string, simple.
             */

            continue;
        }

        /* we seek 8 bytes back, as its length of "kurload\n" and
         * read last 8 bytes to check for end string existance. We
         * don't need to seek back to end of file, as reading will
         * move the pointer by itself.
         */

        lseek(fd, -8, SEEK_CUR);

        if (read(fd, ends, 8) != 8)
        {
            /* totally unexpected, but still we expect it, like a
             * good swat team. We don't know how to recover from
             * this error, so let's call it a day for this client
             */

            el_perror(ELC, "[%3d] couldn't read end string", cfd->fd);
            el_oprint(OELI, "[%s] rejected: end string read error",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        if (strcmp(ends, "kurload\n") != 0)
        {
            /* ending string has not yet been received, we continue
             * getting data from client, and we send information to
             * the client about transfer status. We sent status
             * about progres once per second
             */

            if (last_notif != now)
            {
                server_reply(cfd, "uploaded %10d bytes\n", written);
                last_notif = now;
            }

            continue;
        }

        /* full file received without errors, we even got ending
         * string, we can now break out of loop to perform
         * finishing touch on upload.
         */

        break;
    }

    /* to finish, we need to truncate file, to cut off ending
     * string from file and close it. We carefully check in
     * previous lines that written is at least 8 bytes long,
     * so this subtact is ok.
     */

    written -= 8;
    if (ftruncate(fd, written) != 0)
    {
        el_perror(ELC, "[%3d] couldn't truncate file from ending string",
            cfd->fd);
        el_oprint(OELI, "[%s] rejected: truncate failed",
            inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        goto error;
    }

    /* we will jump to this only when timed upload is enabled and
     * connection has timed out. In that case we don't need (we
     * cannot!) truncate dta by 8 bytes of "kurload\n" string,
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

    if (written == 0)
    {
        el_oprint(OELI, "[%s] rejected: no data has been sent",
            inet_ntoa(client.sin_addr));
        server_reply(cfd, "no data has been sent\n");
        goto error;
    }

    close(fd);

    /* after upload is finished, we send the client, link where he
     * can download his newly uploaded file
     */

    strcpy(url, g_config.domain);
    strcat(url, "/");
    strcat(url, fname);
    el_oprint(OELI, "[%s] %s", inet_ntoa(client.sin_addr), fname);
    server_reply(cfd, "upload complete, link to file %s\n", url);
    server_linger(cfd);
    if (cfd->ssl) ssl_close(cfd->ssl_fd);
    close(cfd->fd);
    free(cfd);

    pthread_mutex_lock(&lconn);
    --cconn;
    pthread_mutex_unlock(&lconn);

    return NULL;

error:
    /* this handles any error during file reception, we remove
     * unfinished upload and close client's connection
     */

    server_linger(cfd);
    if (cfd->ssl) ssl_close(cfd->ssl_fd);
    close(cfd->fd);
    close(fd);
    free(cfd);
    unlink(path);

    pthread_mutex_lock(&lconn);
    --cconn;
    pthread_mutex_unlock(&lconn);

    return NULL;
}


/* ==========================================================================
    in this function we accept connection from the backlog queue, check if
    client is allowed to upload and if server has free upload slots. If all
    checks pass, function starts thread that will handle upload on its own.
   ========================================================================== */


static void server_process_connection
(
    struct fdinfo      *sfd      /* server socket we accept connection from */
)
{
    int                 nconn;   /* current number of active connection */
    struct fdinfo      *cfd;     /* socket associated with connected client */
    int                 flags;   /* flags for cfd socket */
    socklen_t           clen;    /* length of 'client' variable */
    struct sockaddr_in  client;  /* address of remote client */
    pthread_t           t;       /* tread info that will handle upload */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* process all awaiting connections in sfd socket */

    clen = sizeof(client);

    for (;;)
    {
        /* allocate fdinfo, which will be passed to thread */

        cfd = malloc(sizeof(*cfd));

        /* wait for incoming connection */

        if ((cfd->fd = accept(sfd->fd, (struct sockaddr *)&client, &clen)) < 0)
        {
            free(cfd);

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* connection queue in server socket is empty, that
                 * means we processed all queued clients, we can
                 * leave now, our job is done here
                 */

                return;
            }

            el_perror(ELC, "couldn't accept connection");
            el_oprint(OELI, "[NULL] rejected: accept error");
            continue;
        }

        el_print(ELI, "incoming %sssl connection from %s socket id %d",
            sfd->ssl ? "" : "non-", inet_ntoa(client.sin_addr), cfd->fd);

        /* at this point, we still have normal unencrypted
         * connection, so set ssl to 0, so that server_reply()
         * sends possible error data (without any sensitive
         * informations) over non-ssl socket.
         */

        cfd->ssl = 0;

        /* after accepting connection, we have client's ip, now we
         * check if this ip can upload (it can be banned, or not
         * listen in the whitelist, depending on server
         * configuration
         */

        if (bnw_is_allowed(ntohl(client.sin_addr.s_addr)) == 0)
        {
            el_oprint(OELI, "[%s] rejected: not allowed",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "you are not allowed to upload to this server\n");
            if (cfd->ssl) ssl_close(cfd->ssl_fd);
            close(cfd->fd);
            free(cfd);
            continue;
        }

        /* user is allowed to upload, be we still need to check if
         * there is upload slot available (connection limit is not
         * reached)
         */

        pthread_mutex_lock(&lconn);
        nconn = cconn;
        pthread_mutex_unlock(&lconn);

        if (nconn >= g_config.max_connections)
        {
            el_oprint(OELI, "[%s] rejected: connection limit",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "all upload slots are taken, try again later\n");
            if (cfd->ssl) ssl_close(cfd->ssl_fd);
            close(cfd->fd);
            free(cfd);
            continue;
        }

        /* on some systems (like BSDs) socket after accept will
         * inherit flags from accept server socket. In our case we
         * may inherit O_NONBLOCK property which is not what we
         * want. We turn that flag explicitly
         */

        if ((flags = fcntl(cfd->fd, F_GETFL)) == -1)
        {
            el_oprint(OELI, "[%s] rejected: socket config error",
                    inet_ntoa(client.sin_addr));
            el_perror(ELF, "[%3d] error reading socket flags", cfd->fd);
            server_reply(cfd, "internal server error, try again later\n");
            if (cfd->ssl) ssl_close(cfd->ssl_fd);
            close(cfd->fd);
            free(cfd);
            continue;
        }

        if (fcntl(cfd->fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
        {
            el_oprint(OELI, "[%s] rejected: socket config error",
                    inet_ntoa(client.sin_addr));
            el_perror(ELF, "[%3d] error setting socket into block mode",
                    cfd->fd);
            server_reply(cfd, "internal server error, try again later\n");
            if (cfd->ssl) ssl_close(cfd->ssl_fd);
            close(cfd->fd);
            free(cfd);
            continue;
        }

        /* perform ssl handshake, this should be done after fcntl()
         * calls, to make sure cfd->fd is in blocking mode on BSDs,
         * check comment above before fcntl() to know more
         */

        if (sfd->ssl)
        {
            cfd->ssl_fd = ssl_accept(cfd->fd);
            if (cfd->ssl_fd == -1)
            {
                el_oprint(OELI, "[%s] rejected: ssl_accept() error",
                        inet_ntoa(client.sin_addr));

                /* ssl negotation failed, reply in clear text */

                server_reply(cfd, "kurload: ssl negotation failed\n");
                close(cfd->fd);
                free(cfd);
                continue;
            }

            /* now connection is encrypted, note that in clients
             * socket info
             */

            cfd->ssl = 1;
        }

        /* copy information if client should perform timed uploads
         * or not
         */

        cfd->timed = sfd->timed;

        /* client is connected, allowed and connection limit has
         * not been reached, we start thread that will take actions
         * from here.
         */

        if (pthread_create(&t, NULL, server_handle_upload, cfd) != 0)
        {
            el_oprint(OELI, "[%s] rejected: pthread_create error",
                    inet_ntoa(client.sin_addr));
            el_perror(ELC, "[%3d] couldn't start processing thread", cfd->fd);
            server_reply(cfd, "internal server error, try again later\n");
            if (cfd->ssl) ssl_close(cfd->ssl_fd);
            close(cfd->fd);
            free(cfd);
            continue;
        }

        pthread_mutex_lock(&lconn);
        ++cconn;
        pthread_mutex_unlock(&lconn);

        /* we don't need anything from running thread and we surely
         * don't want to babysit it, so we detach and forget about
         * it. Running thread will deal with errors on its own and
         * will terminate in case of any error. That thread is also
         * responsible for freeing cfd.
         */

        pthread_detach(t);
    }
}


/* ==========================================================================
    Functions creates sockets for port for each listen ip (interface)
    specified in config
   ========================================================================== */


static int create_socket_for_ips
(
    int          port,       /* port to create sockets for */
    int          timed,      /* is this timed-enabled upload port? */
    int          ssl,        /* is this ssl port? */
    int          nips,       /* number of ips to listen on*/
    int         *port_index  /* port index being parsed */
)
{
    int          i;          /* current sfds index */
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
        if ((sfds[i].fd = server_create_socket(netip, port)) < 0)
        {
            el_print(ELF, "couldn't create socket for %s:%d", ip, port);
            return -1;
        }

        sfds[i].ssl = ssl;
        sfds[i].timed = timed;

        /* get next ip address on the list */

        ip = strtok(NULL, ",");
    }

    /* increase port index, so next time we write to proper sfds
     * index
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
    int  i;       /* simple iterator */
    int  e;       /* error from function */
    int  pi;      /* current port index */
    int  nports;  /* number of listen ports */
    int  nips;    /* number of ips to listen on*/
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
    nsfds = nports * nips;

    /* allocate memory for all server sockets, one interface equals
     * one server socket.
     */

    if ((sfds = malloc(nsfds * sizeof(*sfds))) == NULL)
    {
        el_print(ELF, "couldn't allocate memory for %d server(s)", nsfds);
        return -1;
    }

    /* initialize mutexes */

    if (pthread_mutex_init(&lconn, NULL) != 0)
    {
        el_perror(ELF, "couldn't initialize current connection mutex");
        free(sfds);
        return -1;
    }

    if (pthread_mutex_init(&lopen, NULL) != 0)
    {
        el_perror(ELF, "couldn't initialize open mutex");
        free(sfds);
        pthread_mutex_destroy(&lconn);
        return -1;
    }

    /* invalidate all allocated server sockets, so closing such
     * socket in case of an error won't crash the app.
     */

    for (i = 0; i != nsfds; ++i)
    {
        sfds[i].fd = -1;
    }

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

    if (g_config.ssl_listen_port)
    {
        /* ssl port enabled, initialize ssl */

        ssl_init();
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
    fd_set  readfds;     /* set containing all server sockets to monitor */
    time_t  prev_flush;  /* time when flush was last called */
    int     maxfd;       /* maximum fd value monitored in readfds */
    int     i;           /* simple iterator for loop */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    prev_flush = 0;
    el_print(ELN, "server initialized and started");

    for (;;)
    {
        int     sact;     /* select activity, just select return value */
        int     i;        /* a simple interator for loop */
        time_t  now;      /* current time from time() */
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

        for (i = 0, maxfd = 0; i != nsfds; ++i)
        {
            /* we validate all sockets in init function, so we are
             * sure sfds is valid and contains valid file
             * descriptors unless user called this function without
             * init or when init failed and return code wasn't
             * checked, then he deserves nice segfault in da face
             */

            FD_SET(sfds[i].fd, &readfds);

            /* we need to find which socket is the highest one,
             * select needs this information to process fds without
             * segfaults
             */

            maxfd = sfds[i].fd > maxfd ? sfds[i].fd : maxfd;
        }

        if (g_shutdown)
        {
            /* shutdown flag was set, program is going to end, we
             * check and return here, right before blocking
             * select() as this has the least chance of locking in
             * select() after SIGTERM
             */

            return;
        }

        /* now we wait for activity, for server sockets (like ours)
         * activity means we have an incoming connection. We pass
         * NULL for timeout, because we want to wait indefinitely,
         * and we are not interested in writefds (we don't write to
         * any socket) nor exceptfds as exceptions don't occur on
         * server sockets.
         */

        sact = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (sact == -1)
        {
            /* an error occured in select, it is most likely EINTR
             * from the SIGTERM signal, in any case, we return so
             * program can finish
             */

            if (errno == EINTR)
            {
                el_print(ELN, "select interrupted by signal");
            }
            else
            {
                el_perror(ELF, "error waiting on socket activity");
            }

            return;
        }

        /* if we get here, that means activity is on any server
         * socket, since we wait indefinietly, select cannot return
         * 0. Now we check which socket got activity and we accept
         * connection to process it.
         */

        for (i = 0; i != nsfds; ++i)
        {
            if (FD_ISSET(sfds[i].fd, &readfds))
            {
                /* well, this socket has something to say, pass it
                 * to processing function to determin what to do
                 * with it
                 */

                server_process_connection(&sfds[i]);
            }

            /* nope, that socket has nothing intereseted going on
             * inside
             */
        }
    }
}

void server_destroy(void)
{
    int              i;    /* simple iterator for loop */
    struct timespec  req;  /* time to sleep in nanosleep() */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* close all server sockets, so any new connection is
     * automatically droped by the system
     */

    for (i = 0; i != nsfds; ++i)
    {
        close(sfds[i].fd);
    }

    pthread_mutex_destroy(&lconn);
    pthread_mutex_destroy(&lopen);
    free(sfds);

    req.tv_sec = 0;
    req.tv_nsec = 100000000l; /* 100[ms] */

    /* when all cleaning is done, we wait for all ongoing
     * transmisions to finish
     */

    el_print(ELN, "waiting for all connections to finish");

    for (;;)
    {
        int nconn;  /* number of currently active connections */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        pthread_mutex_lock(&lconn);
        nconn = cconn;
        pthread_mutex_unlock(&lconn);

        if (nconn == 0)
        {
            /* all connections has been closed, we can proceed with
             * cleaning up operations
             */

            break;
        }

        if (g_stfu)
        {
            /* someone is nervous, finishing without waiting for
             * connection to finish - this might cause file in
             * output_dir to be in invalid state
             */

            el_print(ELW, "exiting without waiting for connection to finish "
                "this may lead to invalid files in %s", g_config.output_dir);

            break;
        }

        nanosleep(&req, NULL);
    }

    if (g_config.ssl_listen_port)
    {
        /* ssl port enabled, cleanup ssl */

        ssl_cleanup();
    }
}

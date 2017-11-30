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
     \                           .       .
      \                         / `.   .' "
       \                .---.  <    > <    >  .---.
        \               |    \  \ - ~ ~ - /  /    |
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
   ========================================================================== */


/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "bnwlist.h"
#include "config.h"
#include "globals.h"
#include "server.h"


/* ==========================================================================
                                   _                __
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
                                   _         __     __
              _   __ ____ _ _____ (_)____ _ / /_   / /___   _____
             | | / // __ `// ___// // __ `// __ \ / // _ \ / ___/
             | |/ // /_/ // /   / // /_/ // /_/ // //  __/(__  )
             |___/ \__,_//_/   /_/ \__,_//_.___//_/ \___//____/

   ========================================================================== */


static size_t           maxfs;  /* maximum size of file that can be uploaded */
static int             *sfds;   /* sfds sockets for all interfaces */
static int              nsfds;  /* number of sfds allocated */
static int              cconn;  /* curently connected clients */
static int              mconn;  /* maximum number of connected clients */
static pthread_mutex_t  lconn;  /* mutex lock for operation on cconn */
static pthread_mutex_t  lopen;  /* mutex for opening file */
static int              maxto;  /* inactivity time after we close connection,
                                   although it is not const, it is const after
                                   initialization by convention */


/* ==========================================================================
                                   _                __
                     ____   _____ (_)_   __ ____ _ / /_ ___
                    / __ \ / ___// /| | / // __ `// __// _ \
                   / /_/ // /   / / | |/ // /_/ // /_ /  __/
                  / .___//_/   /_/  |___/ \__,_/ \__/ \___/
                 /_/
               ____                     __   _
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


/* ==========================================================================
    Function generates random string of length l that is stored in buffer s.
    Caller is responsible for making s big enoug to hold l  +  1  number  of
    bytes.  Returned string contains only numbers and lower-case characters.
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
    formats message pointer by fmt and sends it  all  to  client  associated
    with fd. In case of any error from write function, we just log situation
    but sending is interrupted and client won't receive whole message (if he
    receives anything at all)
   ========================================================================== */


static void server_reply
(
    int          fd,         /* client to send message to */
    const char  *fmt,        /* message format (see printf(3)) */
                 ...         /* variadic arguments for fmt */
)
{
    size_t       written;    /* number of bytes written by write so far */
    size_t       mlen;       /* final size of the message to send */
    char         msg[1024];  /* message to send to the client */
    va_list      ap;         /* variadic argument list from '...' */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    va_start(ap, fmt);
    mlen = vsprintf(msg, fmt, ap);
    va_end(ap);

    /*
     * send reply in loop until all bytes are commited to the kernel for
     * sending
     */

    written = 0;
    while (written != mlen)
    {
        ssize_t  w;  /* number of bytes written by write */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        w = write(fd, msg + written, mlen - written);

        if (w == -1)
        {
            el_perror(ELW, "[%3d] error writing reply to the client", fd);
            return;
        }

        written += w;
    }
}


/* ==========================================================================
    this function creates server socket that  is  fully  configured  and  is
    ready to accept connections
   ========================================================================== */


static int server_create_socket
(
    in_addr_t           ip,     /* local ip to bind server to */
    int                 port    /* port to listen on */
)
{
    int                 fd;     /* new server file descriptor */
    int                 flags;  /* flags for setting socket options */
    struct sockaddr_in  srv;    /* server address to bind to */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        el_perror(ELE, "couldn't create server socket");
        return -1;
    }

    /*
     * as TCP  is  all  about  reliability,  after  server  crashes  (or  is
     * restarted), kernel still keep our server tuple in TIME_WAIT state, to
     * make sure all connections are closed properly disallowing us to  bind
     * to that address again.  We don't need such behaviour, thus  we  allos
     * SO_REUSEADDR
     */

    flags = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) != 0)
    {
        el_perror(ELE, "failed to set socket to SO_REUSEADDR");
        close(fd);
        return -1;
    }

    /*
     * fill server address parameters for listening
     */

    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = ip;

    /*
     * bind socket to srv address, so it only accept connections  from  this
     * ip/interface
     */

    if (bind(fd, (struct sockaddr *)&srv, sizeof(srv)) != 0)
    {
        el_perror(ELE, "failed to bind to socket");
        close(fd);
        return -1;
    }

    /*
     * mark socket to accept incoming  connections.   Backlog  is  set  high
     * enough so that  no  client  can  receive  connection  refused  error.
     */

    if (listen(fd, 256) != 0)
    {
        el_perror(ELE, "failed to make socket to listen");
        close(fd);
        return -1;
    }

    /*
     * set server socket  to  work  in  non  blocking  manner,  all  clients
     * connecting  to  that  socket  will  also  inherit  non  block  nature
     */

    if ((flags = fcntl(fd, F_GETFL)) == -1)
    {
        el_perror(ELE, "error reading socket flags");
        close(fd);
        return INADDR_NONE;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        el_perror(ELE, "error setting socket to O_NONBLOCK");
        close(fd);
        return INADDR_NONE;
    }

    maxfs = cfg_getint(g_config, "max_size");

    return fd;
}


/* ==========================================================================
    This is heart of the swarm... erm I mean of the server. This function is
    a threaded function, it is fired up everytime client connects and passes
    checks for maximum connection, black and white list etc. Function handle
    upload from the client and storing it in the file, it also takes care of
    network problems and end string detection.
   ========================================================================== */


static void *server_handle_upload
(
    void               *arg          /* socket associated with client */
)
{
    struct sockaddr_in  client;      /* address of connected client */
    socklen_t           clen;        /* size of client address */
    sigset_t            set;         /* signals to mask in thread */
    int                 cfd;         /* socket associated with client */
    int                 fd;          /* file where data will be stored */
    int                 ncollision;  /* number of file name collisions hit */
    int                 opathlen;    /* length of output directory path */
    int                 timeout;     /* client inactivity timeout counter */
    char                path[PATH_MAX];  /* full path to the file */
    char                fname[32];   /* random generated file name */
    char                url[1024];   /* generated link to uploaded data */
    char                ends[8 + 1]; /* buffer for end string detection */
    static int          flen = 5;    /* length of the filename to generate */
    unsigned char       buf[8192];   /* temp buffer we read uploaded data to */
    size_t              written;     /* total written bytes to file */
    ssize_t             w;           /* return from write function */
    ssize_t             r;           /* return from read function */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    cfd = (intptr_t)arg;
    clen = sizeof(client);
    getsockname(cfd, (struct sockaddr *)&client, &clen);

    strcpy(path, cfg_getstr(g_config, "output_dir"));
    strcat(path, "/");
    opathlen = strlen(path);

    /*
     * we don't want threads that handle client connection to receive signals,
     * and thus interrupt system calls like write or read. Only main thread
     * can handle signals, so we mask all signals here
     */

    sigfillset(&set);

    if (pthread_sigmask(SIG_SETMASK, &set, NULL) != 0)
    {
        el_perror(ELW, "couldn't mask signals");
        el_oprint(ELI, &g_qlog, "[%s] rejected: signal mask failed",
                inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        return NULL;
    }

    /*
     * generate unique file name for content that client will be sending  we
     * start from file length = 5, and will increase this value  if  we  hit
     * file collision 3 times in a row.   flen  is  static,  because  if  we
     * hit file collision once, there is big enough chance we  will  hit  it
     * again, so to not waste cpu power for checking it  every  single  time
     * client  connects,   we  keep  the  state  in  running  serwer.   When
     * server gets restarted, flen will be reseted back  to  default  value,
     * and increment will again begin from low value.  To  avoid  situation,
     * where 2 threads modify  flen  (leading  to  incrementing  flen  by  2
     * instead of 1), only one thread can open file at a time.   Opening  is
     * fast, so it's not a big deal
     */

    pthread_mutex_lock(&lopen);
    ncollision = 0;
    for (;;)
    {
        server_generate_fname(fname, flen);
        strcpy(path + opathlen, fname);

        if ((fd = open(path, O_CREAT | O_EXCL | O_APPEND | O_RDWR, 0640)) >= 0)
        {
            /*
             * file opened with success, break out of the loop
             */

            break;
        }

        if (errno == EEXIST)
        {
            /*
             * we hit file name collision, increment collision  countr,  and
             * if that counter is bigger than 3, we increment file length by
             * one, because it looks like there are  a  lot  of  files  with
             * current file length
             */

            if (++ncollision == 3)
            {
                ++flen;
                ncollision = 0;
            }

            continue;
        }

        /*
         * unexpected error occured, log situation and close connection
         */

        pthread_mutex_unlock(&lopen);
        el_perror(ELW, "[%3d] couldn't open file %s", cfd, path);
        el_oprint(ELI, &g_qlog, "[%s] rejected: file open error",
                inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        close(cfd);
        pthread_mutex_lock(&lconn);
        --cconn;
        pthread_mutex_unlock(&lconn);
        return NULL;
    }
    pthread_mutex_unlock(&lopen);

    /*
     * file is opened, we can now start to retrieve data from client.  Since
     * purpose of this file server is that user can upload data  using  only
     * simple tools like netcat, we don't have information about file  size.
     * On top of that, we want to send link to upload file, once  upload  is
     * completed, so netcat cannot close connection on upload  complete,  to
     * inform us about upload completed, so we expect an  ending  string  at
     * the end of transfer that will inform us that transfer  is  completed.
     * That ending string is "kurload\n".  Yes, this may lead  to  prepature
     * end  of  transfer,   but  chances  are  so  slim,  we   can   neglect
     * them.
     */

    memset(ends, 0, sizeof(ends));
    written = 0;
    timeout = 0;

    for (;;)
    {
        r = read(cfd, buf, sizeof(buf));

        if (r == -1)
        {
            if (errno == EAGAIN)
            {
                /*
                 * we didn't receive ANY data  within  1  second,  might  be
                 * temporary network problem, or client hunged for a second,
                 * or it could crash,  or  whatever.   One  time  is  not  a
                 * problem, but if problem persists for max timeout (max_to)
                 * in a row, we assume unrecoverable problem  and  we  close
                 * connection without saving uploaded data
                 */

                if (++timeout == maxto)
                {
                    el_print(ELW, "[%3d] client inactive for %d seconds",
                        cfd, maxto);
                    el_oprint(ELI, &g_qlog, "[%s] rejected: inactivity",
                        inet_ntoa(client.sin_addr));

                    /*
                     * this situation can also happen when client uploads file,
                     * but forgot to add ending string "kurload\n", so we
                     * send reply to the client as there is a chance he is
                     * still alive.
                     */

                    server_reply(cfd, "disconnected due to inactivity for %d "
                        "seconds, did you forget to append termination "
                        "string - \"kurload\\n\"?\n", maxto);
                    goto error;
                }

                continue;
            }

            /*
             * else, some different error appeard, we don't want to  recover
             * from it, as it is most probably nore recoverable anyway, just
             * inform client and close connection
             */

            el_perror(ELW, "[%3d] couldn't read from client", cfd);
            el_oprint(ELI, &g_qlog, "[%s] rejected: read error",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        if (r == 0)
        {
            /*
             * Return code 0 means, that client closed connection before all
             * data could be uploaded, well, his choice
             */

            el_oprint(ELI, &g_qlog, "[%s] rejected: connection closed by client",
                inet_ntoa(client.sin_addr));
            goto error;
        }

        if (written + r > maxfs + 8)
        {
            /*
             * we received, in total, more bytes  then  we  can  accept,  we
             * remove such file and return error to the client.  That +8  is
             * for ending string "kurload\n" as we will delete  that  anyway
             * and file will not get more than maxfs size.
             */

            el_oprint(ELI, &g_qlog, "[%s] rejected: file too big",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "file too big, max length is %zu bytes\n", maxfs);
            goto error;
        }

        /*
         * received some data, simply store them into  file,  right  now  we
         * don't care if ending string "kurload\n" ends up  in  a  file,  we
         * will take care of it later.
         */

        if ((w = write(fd, buf, r)) != r)
        {
            el_perror(ELW, "[%3d] couldn't write to file", cfd);
            el_oprint(ELI, &g_qlog, "[%s] rejected: write to file failed",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        /*
         * write was successful, now let's check if  data  written  to  file
         * contains ending string "kurload\n".  For  that  we  read  8  last
         * characters from data stored in file.
         */

        if ((written += w) < 8)
        {
            /*
             * we didn't receive enough bytes to check for ending string, so
             * we don't check for ending string, simple.
             */

            continue;
        }

        /*
         * we seek 8 bytes back, as its length of "kurload\n" and read last
         * 8 bytes to check for end string existance. We don't need to seek
         * back to end of file, as reading will move the pointer itself.
         */

        lseek(fd, -8, SEEK_CUR);

        if (read(fd, ends, 8) != 8)
        {
            /*
             * totally unexpected, but still we expect it, like a good swat
             * team. We don't know how to recover from this error, so let's
             * call it a day for this client
             */

            el_perror(ELW, "[%3d] couldn't read end string", cfd);
            el_oprint(ELI, &g_qlog, "[%s] rejected: end string read error",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "internal server error, try again later\n");
            goto error;
        }

        if (strcmp(ends, "kurload\n") != 0)
        {
            /*
             * ending string has not yet been received, we continue  getting
             * data from client, and we send information to the client about
             * transfer status
             */

            server_reply(cfd, "uploaded %10d bytes\n", written);
            continue;
        }

        /*
         * full file received without errors, we even got ending string,  we
         * can now break out of loop to perform finishing touch  on  upload.
         */

        break;
    }

    /*
     * to finish, we need to truncate file, to cut off  ending  string  from
     * file and close it.
     */

    if (ftruncate(fd, written - 8) != 0)
    {
        el_perror(ELW, "[%3d] couldn't truncate file from ending string", cfd);
        el_oprint(ELI, &g_qlog, "[%s] rejected: truncate failed",
            inet_ntoa(client.sin_addr));
        server_reply(cfd, "internal server error, try again later\n");
        goto error;
    }

    close(fd);

    /*
     * after upload is finished, we send  the  client,  link  where  he  can
     * download his newly uploaded file
     */

    strcpy(url, cfg_getstr(g_config, "domain"));
    strcat(url, "/");
    strcat(url, fname);
    el_oprint(ELI, &g_qlog, "[%s] %s", inet_ntoa(client.sin_addr), fname);
    server_reply(cfd, "upload complete, link to file %s\n", url);
    close(cfd);

    pthread_mutex_lock(&lconn);
    --cconn;
    pthread_mutex_unlock(&lconn);

    return NULL;

error:
    /*
     * this handles any error during file reception, we remove unfinished
     * upload and close client's connection
     */

    close(cfd);
    close(fd);
    unlink(path);

    pthread_mutex_lock(&lconn);
    --cconn;
    pthread_mutex_unlock(&lconn);

    return NULL;
}


/* ==========================================================================
    in this function we accept connection from the backlog queue,  check  if
    client is allowed to upload and if server has free upload slots.  If all
    checks pass, function starts thread that will handle upload on its  own.
   ========================================================================== */


static void server_process_connection
(
    int                 sfd      /* server socket we accept connection from */
)
{
    int                 nconn;   /* current number of active connection */
    int                 cfd;     /* socket associated with connected client */
    socklen_t           clen;    /* length of 'client' variable */
    struct sockaddr_in  client;  /* address of remote client */
    struct timeval      tv;      /* read timeout */
    pthread_t           t;       /* tread info that will handle upload */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * process all awaiting connections in sfd socket
     */

    clen = sizeof(client);

    for (;;)
    {
        if ((cfd = accept(sfd, (struct sockaddr *)&client, &clen)) < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /*
                 * connection queue in server socket is empty, that means
                 * we processed all queued clients, we can leave now, our
                 * job is done here
                 */

                return;
            }

            el_perror(ELW, "couldn't accept connection");
        }

        /*
         * after accepting connection, we have client's ip, now we check
         * if this ip can upload (it can be banned, or not listen in the
         * whitelist, depending on server configuration
         */

        if (bnw_is_allowed(ntohl(client.sin_addr.s_addr)) == 0)
        {
            el_oprint(ELI, &g_qlog, "[%s] rejected: not allowed",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "you are not allowed to upload to this server\n");
            close(cfd);
            continue;
        }

        /*
         * user is allowed to upload, be we still need to check if there
         * is upload slot available (connection limit is not reached)
         */

        pthread_mutex_lock(&lconn);
        nconn = cconn;
        pthread_mutex_unlock(&lconn);

        if (nconn >= mconn)
        {
            el_oprint(ELI, &g_qlog, "[%s] rejected: connection limit",
                inet_ntoa(client.sin_addr));
            server_reply(cfd, "all upload slot are taken, try again later\n");
            close(cfd);
            continue;
        }

        el_print(ELI, "incoming connection from %s socket id %d",
            inet_ntoa(client.sin_addr), cfd);

        /*
         * set timeout so read call on cfd can return if no new data has
         * been received on socket
         */

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        {
            el_perror(ELW, "[%3d] couldn't set timeout for client socket", cfd);
            server_reply(cfd, "internal server error, try again later\n");
            close(cfd);
            continue;
        }

        /*
         * client is connected, allowed and connection limit has not been
         * reached, we start thread that will take actions from here.
         */

        if (pthread_create(&t, NULL, server_handle_upload,
                (void *)(intptr_t)cfd) != 0)
        {
            el_perror(ELW, "[%3d] couldn't start processing thread", cfd);
            server_reply(cfd, "internal server error, try again later\n");
            close(cfd);
            continue;
        }

        pthread_mutex_lock(&lconn);
        ++cconn;
        pthread_mutex_unlock(&lconn);

        /*
         * we don't need anything from running thread and  we  surely  don't
         * want to babysit it, so we detach and forget  about  it.   Running
         * thread will deal with errors on its own  and  will  terminate  in
         * case of any error
         */

        pthread_detach(t);
    }
}


/* ==========================================================================
                                        __     __ _
                         ____   __  __ / /_   / /(_)_____
                        / __ \ / / / // __ \ / // // ___/
                       / /_/ // /_/ // /_/ // // // /__
                      / .___/ \__,_//_.___//_//_/ \___/
                     /_/
               ____                     __   _
              / __/__  __ ____   _____ / /_ (_)____   ____   _____
             / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
            / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
           /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


/* ==========================================================================
    this  creates  server  (or  servers  if  we  will  listen  of   multiple
    interfaces) to handle connections from the clients.  It initializes  all
    memory, structures to valid state. If function returns 0, you're all set
    and you can call loop_forever(), else server is  in  invalid  state  and
    should not run
   ========================================================================== */


int server_init(void)
{
    int     port;
    int     i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    el_print(ELI, "creating server");

    maxto = cfg_getint(g_config, "max_timeout");
    mconn = cfg_getint(g_config, "max_connections");
    nsfds = cfg_size(g_config, "bind_ip");
    port = cfg_getint(g_config, "listen_port");

    /*
     * allocate memory for all server  sockets,  one  interface  equals  one
     * server socket.
     */

    if ((sfds = malloc(nsfds * sizeof(*sfds))) == NULL)
    {
        el_print(ELE, "couldn't allocate memory for %d server(s)", nsfds);
        return -1;
    }

    /*
     * initialize mutexes
     */

    if (pthread_mutex_init(&lconn, NULL) != 0)
    {
        el_perror(ELE, "couldn't initialize current connection mutex");
        free(sfds);
        return -1;
    }

    if (pthread_mutex_init(&lopen, NULL) != 0)
    {
        el_perror(ELE, "couldn't initialize open mutex");
        free(sfds);
        pthread_mutex_destroy(&lconn);
        return -1;
    }

    /*
     * invalidate all allocated server sockets, so closing  such  socket  in
     * case of an error won't crash the app.
     */

    for (i = 0; i != nsfds; ++i)
    {
        sfds[i] = -1;
    }

    /*
     *  Now  we  create  one  server  socket   for   each   interface   user
     *  specified in configuration file.
     */

    for (i = 0; i != nsfds; ++i)
    {
        in_addr_t    netip;  /* ip of the interface to listen on */
        const char  *ip;     /* string representation of IP */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        ip = cfg_getnstr(g_config, "bind_ip", i);
        netip = inet_addr(ip);

        if ((sfds[i] = server_create_socket(netip, port)) < 0)
        {
            el_print(ELE, "couldn't create socket for bind ip %s", ip);
            goto error;
        }
    }

    /*
     * seed random number generator for  generating  unique  file  name  for
     * uploaded files.  We don't need any cryptographic security, so  simple
     * random  seeded  with  current  time  is  more  than  enough  for  us.
     */

    srand(time(NULL));

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
    fd_set  readfds;  /* set containing all server sockets to monitor */
    int     maxfd;    /* maximum fd value monitored in readfds */
    int     i;        /* simple iterator for loop */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * we  may  have  multiple  server  sockets,  so  we  cannot  accept  in
     * blocking fassion.  Since  number  of  server  sockets  will  be  very
     * small, we  can  use  not-so-fast-but-highly-portable  select.   Let's
     * prepare our fdset
     */

    FD_ZERO(&readfds);

    for (i = 0, maxfd = 0; i != nsfds; ++i)
    {
        /*
         * we validate all sockets in init function, so we are sure sfds  is
         * valid and contains valid  file  descriptors  unless  user  called
         * this function  without  init  or  when  init  failed  and  return
         * code wasn't checked, then he deserves nice segfault  in  da  face
         */

        FD_SET(sfds[i], &readfds);

        /*
         * we need to find which socket is the  highest  one,  select  needs
         *   this   information   to   process   fds    without    segfaults
         */

        maxfd = sfds[i] > maxfd ? sfds[i] : maxfd;
    }

    for (;;)
    {
        int     sact;     /* select activity, just select return value */
        int     i;        /* a simple interator for loop */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        if (g_shutdown)
        {
            /*
             * shutdown flag was set, program is going to end, we check and
             * return here, right before blocking select() as this has the
             * least chance of locking in select() after SIGTERM
             */

            return;
        }

        /*
         * now  we  wait  for  activity,  for  server  sockets  (like  ours)
         * activity means we have an incoming connection.  We pass NULL  for
         * timeout, because we want to wait indefinitely,  and  we  are  not
         * interested in  writefds  (we  don't  write  to  any  socket)  nor
         * exceptfds as exceptions don't occur on server sockets.
         */

        sact = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (sact == -1)
        {
            /*
             * an error occured in select, it is most likely EINTR from the
             * SIGTERM signal, in any case, we return so program can finish
             */

            if (errno == EINTR)
            {
                el_print(ELI, "select interrupted by signal");
            }
            else
            {
                el_perror(ELE, "error waiting on socket activity");
            }

            return;
        }

        /*
         * if we get here, that means activity  is  on  any  server  socket,
         * since we wait indefinietly, select cannot return 0.  Now we check
         * which socket got activity and we accept connection to process it.
         */

        for (i = 0; i != nsfds; ++i)
        {
            if (FD_ISSET(sfds[i], &readfds) == 0)
            {
                /*
                 * nope, that socket has nothing intereseted going on inside
                 */

                continue;
            }

            /*
             * well, this socket has something to say, pass it to processing
             * function to determin what to do with it
             */

            server_process_connection(sfds[i]);
        }
    }
}

void server_destroy(void)
{
    int  i;  /* simple iterator for loop */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * close all server sockets, so any new connection is automatically droped
     * by the system
     */

    for (i = 0; i != nsfds; ++i)
    {
        close(sfds[i]);
    }

    pthread_mutex_destroy(&lconn);
    pthread_mutex_destroy(&lopen);
    free(sfds);

    /*
     * when all cleaning is done, we wait for all  ongoing  transmisions  to
     * finish
     */

    el_print(ELI, "waiting for all connections to finish");

    for (;;)
    {
        int nconn;  /* number of currently active connections */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        pthread_mutex_lock(&lconn);
        nconn = cconn;
        pthread_mutex_unlock(&lconn);

        if (nconn == 0)
        {
            /*
             * all connections has been closed, we can proceed with cleaning
             * up operations
             */

            return;
        }

        if (g_stfu)
        {
            /*
             * someone is nervous, finishing without waiting for  connection
             * to finish - this might cause file  in  output_dir  to  be  in
             * invalid state
             */

            el_print(ELW, "exiting without waiting for connection to finish "
                "this may lead to invalid files in %s",
                cfg_getstr(g_config, "output_dir"));

            return;
        }

        usleep(100 * 1000);  /* 100[ms] */
    }
}

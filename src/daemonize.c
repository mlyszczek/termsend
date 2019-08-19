/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -----------------------------------------------------
        / simple daemonizing functions, it handles pid file,  \
        \ dropping priviliges and (who would suspect) forking /
         -----------------------------------------------------
                   \         ,        ,
                    \       /(        )`
                     \      \ \___   / |
                            /- _  `-/  '
                           (/\/ \ \   /\
                           / /   | `    \
                           O O   ) /    |
                           `-^--'`<     '
                          (_.)  _  )   /
                           `.___/`    /
                             `-----' /
                <----.     __ / __   \
                <----|====O)))==) \) /====
                <----'    `--' `.__,' \
                             |        |
                              \       /
                        ______( (_  / \______
                      ,'  ,-----'   |        \
                      `--{__________)        \/
   ==========================================================================
         ____              __                        __               __
        / __/___   ____ _ / /_ __  __ _____ ___     / /_ ___   _____ / /_
       / /_ / _ \ / __ `// __// / / // ___// _ \   / __// _ \ / ___// __/
      / __//  __// /_/ // /_ / /_/ // /   /  __/  / /_ /  __/(__  )/ /_
     /_/   \___/ \__,_/ \__/ \__,_//_/    \___/   \__/ \___//____/ \__/

   ========================================================================== */


/* this definition is needed for ftruncate() and fchown() functions */

#define _XOPEN_SOURCE 500

/* undefine this for solaris or you will be forced to use -std=c89,
 * because solaris will crash just for the sake of crashing. Solaris
 * doesn't allow using c99 compiler when _XOPEN_SOURCE is 500, it
 * just doesn't
 */

#if sun || __sun
#   undef _XOPEN_SOURCE
#endif

/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
    daemonize process, this should be called as early as possible in a
    program. Function does 3 thins:

    - create pid file and store there pid of the forked child, we
      make sure file doesn't exist before we open it (indicating another
      daemon may run in the system)

    - if usr and grp is provided and process is run with root priviliges,
      function will try to drop priviliges to provided usr and grp

    - fork, if fork is successful, parent dies and child returns from this
      function and continues execution

    Function doesn't screw around, if any problem is found, apropriate
    message is printed to stderr and function kills the process without
    playing with resource cleanup (OS will do that). It should be run as
    soon as possible, so there shouldn't be any state needed cleaning when
    daemonizing fails, but if there is, just replace all exit with return
    and change type from void to int.
   ========================================================================== */


void daemonize
(
    const char     *pid_file,  /* path to pid file (ie. /var/run/daemon) */
    const char     *usr,       /* user to drop privilige to */
    const char     *grp        /* group to drop privilige to */
)
{
    struct passwd  *uid;       /* user id associated with usr */
    struct group   *gid;       /* group id associated with grp */
    int             fd;        /* file descriptor of opened pid file */
    pid_t           pid;       /* pid of forked child */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* open file but make sure it doesn't already exists, if it
     * does, it may indicate that another process is running - we
     * don't want to get in a way of already run process, as it may
     * lead to lose of data
     */

    if ((fd = open(pid_file, O_WRONLY | O_CREAT, 0644)) < 0)
    {
        fprintf(stderr, "couldn't create pid file %s, refusing to start: %s\n",
            pid_file, strerror(errno));
        exit(2);
    }

    /* if file exists but is empty, we accept it and belive another
     * daemon is not running. Empty pid file can exist when daemon
     * starts with root permissions (and creates pid file) but when
     * daemon dies, as normal user it may not be able to delete
     * file, in such case it truncates file to be 0 bytes in size
     */

    if (lseek(fd, 0, SEEK_END) > 0)
    {
        /* file exists AND is NOT empty, we assume pid is in there
         */

        fprintf(stderr, "pid file %s already exists, check "
            "if process is running and remove pid file "
            "to start daemon\n", pid_file);
        close(fd);
        exit(1);
    }

    /* if we are root and usr and grp is set, we will be droping
     * priviliges, for the security sake.
     */

    if (usr && grp && getuid() == 0)
    {
        uid = getpwnam(usr);
        gid = getgrnam(grp);

        if (uid == NULL || gid == NULL)
        {
            fprintf(stderr, "couldn't get uid for user: %s group %s: %s\n",
                usr, grp, strerror(errno));
            goto drop_privilige_failed;
        }

        /* since we are droping priviliges, we change ownership of
         * the pid file, so child proces (while not being root
         * anymore) could remove it when going down
         */

        if (fchown(fd, uid->pw_uid, gid->gr_gid) != 0)
        {
            fprintf(stderr, "couldn't chown file %s to %s:%s; %s\n",
                pid_file, usr, grp, strerror(errno));
            goto drop_privilige_failed;
        }

        /* finally, drop priviliges. We need to set gid first,
         * because if we drop uid first, we won't be able to drop
         * to gid (we are not root anymore)
         */

        if (setgid(gid->gr_gid) != 0)
        {
            fprintf(stderr, "couldn't set gid to %s %s\n", grp,
                strerror(errno));
            goto drop_privilige_failed;
        }

        if (setuid(uid->pw_uid) != 0)
        {
            fprintf(stderr, "couldn't set uid to %s %s\n", usr,
                strerror(errno));
            goto drop_privilige_failed;
        }

        goto drop_privilige_finished;

    drop_privilige_failed:
        close(fd);
        unlink(pid_file);
        exit(2);
    }

drop_privilige_finished:
    /* now that we know we can start daemon, let's fork - that
     * means we make our very own child, a sibling process with its
     * own address space, own file descriptors, own everything.
     * Parent process and child process share almost nothing, but
     * child inherits all parent's memory, file descriptors and
     * some more stuff (look at man fork(2)), and then they just
     * drift apart just after conceive
     */

    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "forking failed: %s\n", strerror(errno));
        close(fd);
        unlink(pid_file);
        exit(2);
    }

    if (pid > 0)
    {
        char  pids[32];  /* pid as a string */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        /* only parent will get here, and all it does is to
         * sacrifice itself so the child may live and carry on,
         * what he couldn't. But before he does that, parent stores
         * its child PID into file, so others can find him and kill
         * him if he missbehaves or is no longer needed
         */

        sprintf(pids, "%d", pid);

        if (write(fd, pids, strlen(pids)) != (ssize_t)strlen(pids))
        {
            kill(pid, SIGKILL);
            fprintf(stderr, "error writing pid to file %s: %s\n", pid_file,
                strerror(errno));
        }

        close(fd);
        exit(0);
    }

    /* since child is forked to the backgroud, and does not have any
     * controlling terminal (if it ever had one) we close stdout
     * and stderr. We do this reopening both streams to /dev/null,
     * as writing to closed FILE is UB. freopen() will close original
     * file descriptor detaching any process that might be reading
     * them (like cron)
     */

    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    /* ok, cool, now that we got rid of parent, the fun can start.
     * We return from the function to our daemon code, since
     * everything is already done.
     *
     *   - pid file created - [check]
     *   - droped priviliges - [check]
     *   - forked with success - [check]
     */
}


/* ==========================================================================
    clean up whatever daemonize function did
   ========================================================================== */


void daemonize_cleanup
(
    const char  *pid_file
)
{
    if (unlink(pid_file) == 0)
    {
        /* pid file deleted, nothing else to be done */

        return;
    }

    /* pid file couldn't be deleted, we don't have access to write
     * to directory where pid file exists, but we still should be
     * able to write to pid file itself, so empty the file to
     * indicate daemon doesn't run anymore
     */

    if (truncate(pid_file, 0) != 0)
    {
        fprintf(stderr, "could not remove pid file %s "
            "nor we could truncate pid file to 0 bytes %s\n",
            pid_file, strerror(errno));
    }
}

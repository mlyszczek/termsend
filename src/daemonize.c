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
   ========================================================================== */


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
    daemonize process, this should be called  as  early  as  possible  in  a
    program. Function does 3 thins:

      - create  pid  file and store  there  pid  of  the  forked  child,  we
        make sure file doesn't exist before we open it  (indicating  another
        daemon may run in the system)

      - if usr and grp is provided  and process is run with root priviliges,
        function will try to drop priviliges to provided usr and grp

      - fork, if fork is successful, parent dies and child returns from this
        function and continues execution

    Function doesn't screw around,  if  any  problem  is  found,  apropriate
    message is printed to stderr and  function  kills  the  process  without
    playing with resource cleanup (OS will do that).  It should  be  run  as
    soon as possible, so there shouldn't be any state needed  cleaning  when
    daemonizing fails, but if there is, just replace all  exit  with  return
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


    /*
     * open file but make sure it doesn't already exists, if it does, it may
     * indicate that another process is running - we don't want to get in  a
     * way of  already  run  process,  as  it  may  lead  to  lose  of  data
     */

    if ((fd = open(pid_file, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "pid file %s already exists, check if process is "
                "running and remove pid file to start daemon\n", pid_file);
            exit(1);
        }

        fprintf(stderr, "couldn't create pid file %s, refusing to start: %s\n",
            pid_file, strerror(errno));
        exit(2);
    }

    /*
     * if we are root and usr and grp is set, we will be droping priviliges,
     * for the security sake.
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

        /*
         * since we are droping priviliges, we change ownership of  the  pid
         * file, so child  proces  (while  not  being  root  anymore)  could
         * remove  it  when  going down
         */

        if (fchown(fd, uid->pw_uid, gid->gr_gid) != 0)
        {
            fprintf(stderr, "couldn't chown file %s to %s:%s; %s\n",
                pid_file, usr, grp, strerror(errno));
            goto drop_privilige_failed;
        }

        /*
         * finally, drop priviliges. We need to set gid first, because if we
         * drop uid first, we won't be able to drop to gid (we are not  root
         * anymore)
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
    /*
     * now that we know we can start daemon, let's fork - that means we make
     * our very own child, a sibling process with its own address space, own
     * file descriptors, own everything.  Parent process and  child  process
     * share almost nothing, but child inherits all  parent's  memory,  file
     * descriptors and some more stuff (look at man fork(2)), and then  they
     * just drift apart just after conceive
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


        /*
         * only parent will get here, and all it does is to sacrifice itself
         * so the child may live and carry on, what he couldn't.  But before
         * he does that, parent stores its child PID into  file,  so  others
         * can find him and kill him if  he  missbehaves  or  is  no  longer
         * needed
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

    /*
     * ok, cool, now that we got rid of parent, the fun can start. We return
     * from the function to our daemon code, since everything is already done.
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
    if (unlink(pid_file) != 0)
    {
        fprintf(stderr, "couldn't remove pid file %s: %s\n", pid_file,
            strerror(errno));
    }
}

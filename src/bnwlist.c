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


#include <embedlog.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "bnwlist.h"


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


in_addr_t  *ip_list;  /* list containing IPs, black or white */
int         mode;     /* operation mode, 0 - none, -1 blacklist, 1 whitelist */


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

   ========================================================================== */


static int bnw_parse_list
(
    char  *f,
    off_t  flen
)
{
    size_t  n;
    int     i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * first count number of entries so we can allocate enough memory
     */

    for (i = 0; i != flen; ++i)
    {
        n += f[i] == '\n';
    }

    /*
     * we reserve space for n + 1 elements, for NULL at the end
     */

    ++n;
    if ((ip_list = malloc(n * sizeof(in_addr_t))) == NULL)
    {
        el_print(ELE, "malloc error %d bytes for list", n * sizeof(in_addr_t));
        return -1;
    }

    /*
     * no we can parse file and convert string IPs into small in_addr_t
     */

    for (i = 0, n = 0; i != flen; ++i, ++n)
    {
        int   j;           /* iterator for loop */
        char  ip[15 + 1];  /* 123.123.123.123 => 3 * 4 + 3 + null */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        /*
         * copy next address into ip buffer
         */

        for (j = 0; f[i] != '\n'; ++j, ++i)
        {
            if (j == 15)
            {
                el_print(ELE, "error parsing list file in line %d", n + 1);
                free(ip_list);
                return -1;
            }

            ip[j] = f[i];
        }

        /*
         * null terminate ip, and convert string to in_addr_t. '\n' in f will be
         * skiped inside for loop
         */

        ip[j] = '\0';

        if ((ip_list[n] = inet_addr(ip)) == INADDR_NONE)
        {
            el_print(ELE, "malformed ip (%s) in list on line %d", ip, n + 1);
            free(ip_list);
            return -1;
        }

        el_print(ELD, "adding ip to list: %s", ip);
    }

    /*
     * null terminate ip_list
     */

    ip_list[n] = (in_addr_t)0;

    return 0;
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


int bnw_init
(
    const char  *flist,  /* path to file with IPs list to parse */
    int          m       /* operation mode */
)
{
    char        *f;      /* pointer to mmaped flist file */
    int          fd;     /* opened flist file descriptor */
    struct stat  st;     /* information about flist file */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if ((mode = m) == 0)
    {
        el_print(ELI, "ip filtering is off");
        return 0;
    }

    el_print(ELI, "loading list file %s", flist);

    if (stat(flist, &st) == -1)
    {
        if (errno = ENOENT)
        {
            el_print(ELW, "file list doesn't exist, assuming no filter");
            mode = 0;
            return 0;
        }

        el_perror(ELE, "couldn't stat list file");
        return -1;
    }

    if ((fd = open(flist, O_RDONLY)) == -1)
    {
        el_perror(ELE, "coudln't open list file");
        return -1;
    }

    f = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (f == MAP_FAILED)
    {
        el_perror(ELE, "couldn't mmap flist file");
        close(fd);
        return -1;
    }

    if (bnw_parse_list(f, st.st_size) == -1)
    {
        el_print(ELE, "parsing list failed");
        munmap(f, st.st_size);
        close(fd);
        return -1;
    }

    munmap(f, st.st_size);
    close(fd);
    return 0;
}

/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================

   ------------------------------------------------------------
  / This module is responsible for loading black or white list \
  | from file, parse it and convert to sorted array of         |
  | in_addr_t. It also allows to check if given IP address is  |
  \ allowed, depening on mode, to upload or not                /
   ------------------------------------------------------------
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


#include <arpa/inet.h>
#include <embedlog.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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


static in_addr_t  *ip_list;  /* list containing IPs in host endianess */
static size_t      num_ip;   /* number of ips in ip_list */
static int         mode;     /* operation mode, 0 - none, -1 black, 1 white */


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
    comparator for sorting ip list
   ========================================================================== */


static int bnw_ip_comp
(
    const void       *a,    /* ip a */
    const void       *b     /* ip b */
)
{
    const in_addr_t  *ipa;  /* ip a */
    const in_addr_t  *ipb;  /* ip b */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    ipa = a;
    ipb = b;

    if (*ipa == *ipb)
    {
        return 0;
    }

    if (*ipa < *ipb)
    {
        return -1;
    }

    return 1;
}


/* ==========================================================================
    function parses file with  list  and  converts  IPs  there  from  string
    representation ("127.0.0.1") to in_addr_t type (uint32_t).   Parsed  IPs
    are stored in heap allocated memory 'ip_list'.  If sytax error is  found
    in list file, nothing is allocated and -1 is returned.
   ========================================================================== */


static int bnw_parse_list
(
    char  *f,    /* memory mapped list file */
    off_t  flen  /* length of f buffer */
)
{
    size_t  n;   /* number of entries in file */
    int     i;   /* helper iterator for loop */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /*
     * first count number of entries so we can allocate enough memory
     */

    for (i = 0, n = 0; i != flen; ++i)
    {
        n += f[i] == '\n';
    }

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

        if ((ip_list[n] = ntohl(inet_addr(ip))) == INADDR_NONE)
        {
            el_print(ELE, "malformed ip (%s) in list on line %d", ip, n + 1);
            free(ip_list);
            return -1;
        }

        el_print(ELD, "adding ip to list: %s", ip);
    }

    /*
     * sort IPs for faster search
     */

    num_ip = n;
    qsort(ip_list, n, sizeof(in_addr_t), bnw_ip_comp);
    el_print(ELI, "%d IPs added to the list, list size in mem %zu bytes",
        n, n * sizeof(in_addr_t));

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


/* ==========================================================================
    initializes all private data in this module. It allocates memory for
    IP list from file, and loads list to private ip_list variable. If mode
    is set to 0 (no filtering) no data is allocated.
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


/* ==========================================================================
    determines wheter ip should be allowed to upload to server or not.
   ========================================================================== */


int bnw_is_allowed
(
    in_addr_t ip      /* ip address to check */
)
{
    int       begin;  /* begin index for binary search */
    int       end;    /* end index for binary search */
    int       i;      /* middle index to check for binary search */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if (mode == 0)
    {
        /*
         * filtering is off, ip is always allowed
         */

        return 1;
    }

    ip = ntohl(ip);
    begin = 0;
    end = num_ip - 1;

    while (begin <= end)
    {
        i = (begin + end) / 2;

        if (ip_list[i] == ip)
        {
            if (mode == 1)
            {
                /*
                 * ip was found in white list mode, ip is allowed
                 */

                return 1;
            }
            else
            {
                /*
                 * ip was found in black list mode, ip is not allowed
                 */

                return 0;
            }
        }

        if (ip_list[i] > ip)
        {
            end = i - 1;
        }
        else
        {
            begin = i + 1;
        }
    }

    if (mode == 1)
    {
        /*
         * ip was not found in white list, do not allow
         */

        return 0;
    }
    else
    {
        /*
         * ip was not found in black list, allow ip
         */

        return 1;
    }
}


/* ==========================================================================
    frees all resources allocated by this module
   ========================================================================== */


void bnw_destroy(void)
{
    free(ip_list);
}

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


#include "mtest.h"
#include "bnwlist.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


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

#define IPTESTNUM 1000
#define BNWFILE "/tmp/kurload-test-bnwlist"
mt_defs_ext();

static int bnwfd;


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


static void test_prepare(void)
{
    bnwfd = open(BNWFILE, O_CREAT | O_TRUNC | O_RDWR, 0644);
    srand(time(NULL));
}


static void test_cleanup(void)
{
    close(bnwfd);
    bnw_destroy();
    unlink(BNWFILE);
}


static void add_ip
(
    const char  *ip
)
{
    (void) write(bnwfd, ip, strlen(ip));
    (void) write(bnwfd, "\n", 1);
}


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */



static void bnw_no_list(void)
{
    mt_fok(bnw_init(NULL, 0));
}


/* ==========================================================================
   ========================================================================== */


static void bnw_one_ip(void)
{
    add_ip("10.1.1.2");
    mt_fok(bnw_init(BNWFILE, 1));
}


/* ==========================================================================
   ========================================================================== */


static void bnw_multiple_ip(void)
{
    add_ip("10.1.1.2");
    add_ip("10.1.1.5");
    add_ip("12.1.1.1");
    mt_fok(bnw_init(BNWFILE, 1));
}


/* ==========================================================================
   ========================================================================== */


static void bnw_single_ip_whitelist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    int        allowed;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    add_ip("10.1.1.2");
    mt_fok(bnw_init(BNWFILE, 1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 0);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        allowed = ip == inet_addr("10.1.1.2") ? 1 : 0;
        mt_fail(bnw_is_allowed(ip) == allowed);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_multi_ip_whitelist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    int        allowed;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    add_ip("10.1.1.2");
    add_ip("32.14.26.1");
    add_ip("192.168.1.6");
    mt_fok(bnw_init(BNWFILE, 1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.2")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.0")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.6")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.5")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.7")) == 0);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        allowed = ip == inet_addr("10.1.1.2") ||
            ip == inet_addr("32.14.26.1") ||
            ip == inet_addr("192.168.1.6")
            ? 1 : 0;
        mt_fail(bnw_is_allowed(ip) == allowed);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_empty_list_whitelist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fok(bnw_init(BNWFILE, 1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 0);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        mt_fail(bnw_is_allowed(ip) == 0);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_single_ip_blacklist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    int        allowed;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    add_ip("10.1.1.2");
    mt_fok(bnw_init(BNWFILE, -1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 1);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        allowed = ip == inet_addr("10.1.1.2") ? 0 : 1;
        mt_fail(bnw_is_allowed(ip) == allowed);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_multi_ip_blacklist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    int        allowed;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    add_ip("10.1.1.2");
    add_ip("32.14.26.1");
    add_ip("192.168.1.6");
    mt_fok(bnw_init(BNWFILE, -1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.1")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("32.14.26.0")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.6")) == 0);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.5")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("192.168.1.7")) == 1);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        allowed = ip == inet_addr("10.1.1.2") ||
            ip == inet_addr("32.14.26.1") ||
            ip == inet_addr("192.168.1.6")
            ? 0 : 1;
        mt_fail(bnw_is_allowed(ip) == allowed);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_empty_list_blacklist_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fok(bnw_init(BNWFILE, -1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 1);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        mt_fail(bnw_is_allowed(ip) == 1);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_no_list_is_allowed(void)
{
    in_addr_t  ip;
    int        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fok(bnw_init(NULL, 0));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 1);

    for (i = 0; i != IPTESTNUM; ++i)
    {
        ip = rand();
        mt_fail(bnw_is_allowed(ip) == 1);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_totally_random_test(void)
{
#define NUMIPS 1000
    int        num_checks = 100000;
    in_addr_t  ips[NUMIPS];
    in_addr_t  ip;
    int        i;
    int        j;
    int        allowed;
    char       sip[15 + 1];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (i = 0; i != NUMIPS; ++i)
    {
        ips[i] = rand();
        inet_ntop(AF_INET, &ips[i], sip, sizeof(sip));
        add_ip(sip);
    }

    bnw_init(BNWFILE, 1);

    for (i = 0; i != num_checks; ++i)
    {
        ip = rand();
        allowed = 0;

        for (j = 0; j != NUMIPS; ++j)
        {
            if (ip == ips[j])
            {
                allowed = 1;
                break;
            }
        }

        mt_fail(bnw_is_allowed(ip) == allowed);
    }
}


/* ==========================================================================
   ========================================================================== */


static void bnw_null_list_enabled(void)
{
    mt_ferr(bnw_init(NULL, 1), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_list_bad_ip_missing_last_octet(void)
{
    add_ip("10.1.1.1");
    add_ip("10.1.1.");
    add_ip("10.1.1.3");

    mt_ferr(bnw_init(BNWFILE, 1), EFAULT);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_list_bad_ip_too_much_octets(void)
{
    add_ip("10.1.1.1");
    add_ip("10.1.1.1.2");
    add_ip("10.1.1.3");

    mt_ferr(bnw_init(BNWFILE, 1), EFAULT);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_list_bad_ip_too_big_octet(void)
{
    add_ip("10.1.1.1");
    add_ip("10.1.325.1");
    add_ip("10.1.1.3");

    mt_ferr(bnw_init(BNWFILE, 1), EFAULT);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_list_bad_ip_two_ips_in_one_row(void)
{
    add_ip("10.1.1.1");
    add_ip("10.1.1.5 10.1.1.2");
    add_ip("10.1.1.3");

    mt_ferr(bnw_init(BNWFILE, 1), EFAULT);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_list_bad_ip_totally_not_an_ip(void)
{
    add_ip("10.1.1.1");
    add_ip("ehehe xD");
    add_ip("10.1.1.3");

    mt_ferr(bnw_init(BNWFILE, 1), EFAULT);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_non_existing_list(void)
{
    mt_fok(bnw_init("/path/that/doesnt/exist", 1));
}


/* ==========================================================================
   ========================================================================== */


static void bnw_no_read_access(void)
{
    add_ip("10.1.1.1");
    add_ip("10.1.1.2");
    chmod(BNWFILE, 0200);
    mt_ferr(bnw_init(BNWFILE, 1), EACCES);
}


/* ==========================================================================
   ========================================================================== */


static void bnw_empty_lines_in_list(void)
{
    add_ip("");
    add_ip("10.1.1.1");
    add_ip("");
    add_ip("10.1.1.2");
    add_ip("");
    add_ip("");
    add_ip("");
    add_ip("10.1.1.3");
    add_ip("");

    mt_fok(bnw_init(BNWFILE, 1));
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.1")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.2")) == 1);
    mt_fail(bnw_is_allowed(inet_addr("10.1.1.3")) == 1);

}



/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void bnwlist_test_group()
{
    mt_prepare_test = &test_prepare;
    mt_cleanup_test = &test_cleanup;

    mt_run(bnw_no_list);
    mt_run(bnw_one_ip);
    mt_run(bnw_multiple_ip);
    mt_run(bnw_single_ip_whitelist_is_allowed);
    mt_run(bnw_multi_ip_whitelist_is_allowed);
    mt_run(bnw_empty_list_whitelist_is_allowed);
    mt_run(bnw_single_ip_blacklist_is_allowed);
    mt_run(bnw_multi_ip_blacklist_is_allowed);
    mt_run(bnw_empty_list_blacklist_is_allowed);
    mt_run(bnw_no_list_is_allowed);
    mt_run(bnw_totally_random_test);
    mt_run(bnw_null_list_enabled);
    mt_run(bnw_list_bad_ip_missing_last_octet);
    mt_run(bnw_list_bad_ip_too_much_octets);
    mt_run(bnw_list_bad_ip_too_big_octet);
    mt_run(bnw_list_bad_ip_two_ips_in_one_row);
    mt_run(bnw_list_bad_ip_totally_not_an_ip);
    mt_run(bnw_non_existing_list);
    mt_run(bnw_no_read_access);
    mt_run(bnw_empty_lines_in_list);
}

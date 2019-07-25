/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifdef HAVE_CONFIG_H
#   include "kurload.h"
#endif

#include "mtest.h"
#include "ssl/ssl.h"
#include "test-group-list.h"

#include <errno.h>

mt_defs();

#if HAVE_SSL == 0
static void test_check_ssl_enosys(void)
{
    mt_ferr(ssl_init(), ENOSYS);
}
#endif

int main(void)
{
    bnwlist_test_group();
    config_test_group();
#if HAVE_SSL == 0
    mt_run(test_check_ssl_enosys);
#endif

    mt_return();
}

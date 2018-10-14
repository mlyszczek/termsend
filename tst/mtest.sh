#!/bin/sh

## ==========================================================================
#   Licensed under BSD 2clause license See LICENSE file for more information
#   Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
#  ==========================================================================
#    __________________________________________________________
#   /                   mtest version v1.1.3                   \
#   |                                                          |
#   |    Simple test framework that uses TAP output format     |
#   \                 http://testanything.org                  /
#    ----------------------------------------------------------
#          \
#           \
#            \        .
#             .---.  //
#            Y|o o|Y//
#           /_(i=i)K/
#           ~()~*~()~
#            (_)-(_)
#
#        Darth Vader Koala
## ==========================================================================


## ==========================================================================
#                                   __               __
#                       ____ ___   / /_ ___   _____ / /_
#                      / __ `__ \ / __// _ \ / ___// __/
#                     / / / / / // /_ /  __/(__  )/ /_
#                    /_/ /_/ /_/ \__/ \___//____/ \__/
#
#                                  _         __     __
#             _   __ ____ _ _____ (_)____ _ / /_   / /___   _____
#            | | / // __ `// ___// // __ `// __ \ / // _ \ / ___/
#            | |/ // /_/ // /   / // /_/ // /_/ // //  __/(__  )
#            |___/ \__,_//_/   /_/ \__,_//_.___//_/ \___//____/
#
## ==========================================================================


mt_test_status=0
mt_total_tests=0
mt_total_failed=0
mt_total_checks=0
mt_checks_failed=0
mt_current_test="none"


## ==========================================================================
#                                       __     __ _
#                        ____   __  __ / /_   / /(_)_____
#                       / __ \ / / / // __ \ / // // ___/
#                      / /_/ // /_/ // /_/ // // // /__
#                     / .___/ \__,_//_.___//_//_/ \___/
#                    /_/
#              ____                     __   _
#             / __/__  __ ____   _____ / /_ (_)____   ____   _____
#            / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
#           / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
#          /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/
#
## ==========================================================================


## ==========================================================================
#   run specified test
#
#   $1 - function name as a string - will be passed to eval
## ==========================================================================


mt_run()
{
    mt_run_named $1 $1
}


## ==========================================================================
#   run specified test with custom name to be printed during report
#
#   $1 - function name as a string - will be passed to eval
#   $2 - test name, will be used instead of $1 in report
## ==========================================================================


mt_run_named()
{
    mt_current_test="$2"
    mt_test_status=0
    mt_total_tests=$((mt_total_tests + 1))

    if type mt_prepare_test > /dev/null 2>&1
    then
        mt_prepare_test
    fi

    eval "$1"

    if type mt_cleanup_test > /dev/null 2>&1
    then
        mt_cleanup_test
    fi

    if [ $mt_test_status -ne 0 ]
    then
        echo "not ok $mt_total_tests - $mt_current_test"
        mt_total_failed=$((mt_total_failed + 1))
    else
        echo "ok $mt_total_tests - $mt_current_test"
    fi
}


## ==========================================================================
#   performs check on given command, if command returns error,  current test
#   will be marked as failed.
#
#   $1 - code to evaluate, simply passed to eval
## ==========================================================================


mt_fail()
{
    mt_total_checks=$(( mt_total_checks + 1 ))
    if ! eval $1
    then
        echo "# assert $mt_current_test, '$1'"
        mt_test_status=1
        mt_checks_failed=$(( mt_checks_failed + 1 ))
    fi
}


## ==========================================================================
#   prints test plant in  format 1..<number_of_test_run>.  If all tests have
#   passed,  macro will exit script with code 0,  else  it returns number of
#   failed tests.  If number of failed tests  exceeds 254,  then 254 will be
#   returned.
#
#   This function should be called when all tests have been run
## ==========================================================================


mt_return()
{
    echo "1..$mt_total_tests"

    mt_passed_tests=$((mt_total_tests - mt_total_failed))
    mt_passed_checks=$((mt_total_checks - mt_checks_failed))

    printf "# total tests.......: %4d\n" ${mt_total_tests}
    printf "# passed tests......: %4d\n" ${mt_passed_tests}
    printf "# failed tests......: %4d\n" ${mt_total_failed}
    printf "# total checks......: %4d\n" ${mt_total_checks}
    printf "# passed checks.....: %4d\n" ${mt_passed_checks}
    printf "# failed checks.....: %4d\n" ${mt_checks_failed}

    if [ $mt_total_failed -gt 254 ]
    then
        exit 254
    else
        exit $mt_total_failed
    fi
}

#!/usr/bin/env bash


updir="./kurload-test/out"
data="./kurload-test/data"

. ./mtest.sh
os="$(uname)"
if [ "${os}" = "Linux" ]
then
    server="127.$((RANDOM % 256)).$((RANDOM % 256)).$((RANDOM % 254 + 2))"
else
    server="127.0.0.1"
fi

if [ "${os}" = "SunOS" ]
then
    nc="nc -F"
else
    nc="nc"
fi

## ==========================================================================
#                                  _                __
#                    ____   _____ (_)_   __ ____ _ / /_ ___
#                   / __ \ / ___// /| | / // __ `// __// _ \
#                  / /_/ // /   / / | |/ // /_/ // /_ /  __/
#                 / .___//_/   /_/  |___/ \__,_/ \__/ \___/
#                /_/
#              ____                     __   _
#             / __/__  __ ____   _____ / /_ (_)____   ____   _____
#            / /_ / / / // __ \ / ___// __// // __ \ / __ \ / ___/
#           / __// /_/ // / / // /__ / /_ / // /_/ // / / /(__  )
#          /_/   \__,_//_/ /_/ \___/ \__//_/ \____//_/ /_//____/
#
## ==========================================================================


mt_prepare_test()
{
    mkdir -p ./kurload-test/out
    ../src/kurload -D -l6 -c -i61337 -s1024 -t1 -m3 -dlocalhost -ukurload \
        -gkurload -P./kurload-test/kurload.pid \
        -q./kurload-test/kurload-query.log -p./kurload-test/kurload.log \
        -L./kurload-test/blacklist -T-1 -o./kurload-test/out \
        -b${server}
    sleep 1
}


mt_cleanup_test()
{
    kill -15 `cat ./kurload-test/kurload.pid`
    sleep 1
    rm -rf ./kurload-test
}


## ==========================================================================
#   Appends "kurload\n" to standard output and sends message to kurload
## ==========================================================================


kurload()
{
    cat - <(echo 'kurload') | ${nc} ${server} 61337 2>/dev/null
}


## ==========================================================================
#   Parses output from kurload serveri to get generated file name where data
#   was stored in
## ==========================================================================


get_file()
{
    tail -n1 | rev | cut -d/ -f-1 | rev
}


## ==========================================================================
#   Generate random string. Function appends '\n' automatically, to generate
#   string with 128 byte (including \n") pass 127 as first argument.
#
#   $1 - number of characters to generate (excluding '\n'
## ==========================================================================


randstr()
{
    cat /dev/urandom | tr -dc 'a-zA-Z0-9' | tr -d '\0' | fold -w $1 | head -n 1
}


## ==========================================================================
#   Generates random binary data
#
#   $1 - number of bytes to generate
## ==========================================================================


randbin()
{
    dd if=/dev/urandom bs=1 count=${1} 2>/dev/null
}


## ==========================================================================
#   verifyes whether data upload to server was successfull. This function is
#   thread-safe.
## ==========================================================================


multi_thread_check()
{
    fname="$(randstr 120)"
    randstr 128 > "${data}.${fname}"
    out="$(cat "${data}.${fname}" | kurload | tail -n1)"

    if [ "${out}" = "all upload slots are taken, try again later" ]
    then
        return 0
    elif [[ "${out}" = "upload complete, link to file localhost/"* ]]
    then
        file="$(echo "${out}" | rev | cut -d/ -f-1 | rev)"
        mt_fail "diff ${updir}/${file} ${data}.${fname}"
        return $?
    else
        if [ -z "${out}" ]
        then
            return
        fi

        echo "something weird received: '${out}'"
        mt_fail false
        return 0
    fi
}


## ==========================================================================
#                          __               __
#                         / /_ ___   _____ / /_ _____
#                        / __// _ \ / ___// __// ___/
#                       / /_ /  __/(__  )/ /_ (__  )
#                       \__/ \___//____/ \__//____/
#
## ==========================================================================


test_is_running()
{
    mt_fail "kill -s 0 `cat ./kurload-test/kurload.pid`"
}


## ==========================================================================
## ==========================================================================


test_send_string()
{
    randstr 128 > $data
    file=`cat $data | kurload | get_file`
    mt_fail "diff $updir/$file $data"
}


test_threaded()
{
    for i in $(seq 1 1 16)
    do
        multi_thread_check &
    done
    sleep 10
}

## ==========================================================================
## ==========================================================================


test_send_string_full()
{
    randstr 1023 > $data
    file=`cat $data | kurload | get_file`
    mt_fail "diff $updir/$file $data"
}


## ==========================================================================
## ==========================================================================


test_send_string_too_big()
{
    randstr 1337 > $data
    out=`cat $data | kurload | tail -n1`
    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_bin()
{
    randbin 128 > $data
    file=`cat $data | kurload | get_file`
    mt_fail "diff $updir/$file $data"
}


## ==========================================================================
## ==========================================================================


test_send_bin_full()
{
    randbin 1024 > $data
    file=`cat $data | kurload | get_file`
    mt_fail "diff $updir/$file $data"
}


## ==========================================================================
## ==========================================================================


test_send_bin_too_big()
{
    randbin 1337 > $data
    out=`cat $data | kurload | tail -n1`
    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_and_timeout()
{
    randbin 128 > $data
    out=`cat $data | ${nc} ${server} 61337 2> /dev/null | tail -n1`
    mt_fail "[ \"$out\" == \"disconnected due to inactivity for 1 seconds, did you forget to append termination string - \"kurload\\n\"?\" ]"
}


## ==========================================================================
## ==========================================================================


test_totally_random()
{
    for i in `seq 1 1 128`
    do
        numbytes=$((RANDOM % 2048))
        finish=$((RANDOM % 32))


        if [ $finish -eq 0 ]
        then
            if [ $numbytes -gt 1024 ]
            then
                numbytes=512
            fi

            randbin $numbytes > $data
            out=`cat $data | ${nc} ${server} 61337 2> /dev/null | tail -n1`

            mt_fail "[ \"$out\" == \"disconnected due to inactivity for 1 seconds, did you forget to append termination string - \"kurload\\n\"?\" ]"
        else
            randbin $numbytes > $data

            if [ $numbytes -gt 1024 ]
            then
                out=`cat $data | kurload | tail -n1`
                if [ ! -z "$out" ]
                then
                    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
                fi
            else
                file=`cat $data | kurload | get_file`
                mt_fail "diff $updir/$file $data"
            fi
        fi

    done
}


## ==========================================================================
#   __               __                                    __   _
#  / /_ ___   _____ / /_   ___   _  __ ___   _____ __  __ / /_ (_)____   ____
# / __// _ \ / ___// __/  / _ \ | |/_// _ \ / ___// / / // __// // __ \ / __ \
#/ /_ /  __/(__  )/ /_   /  __/_>  < /  __// /__ / /_/ // /_ / // /_/ // / / /
#\__/ \___//____/ \__/   \___//_/|_| \___/ \___/ \__,_/ \__//_/ \____//_/ /_/
#
## ==========================================================================


mt_run test_is_running
mt_run test_send_string
mt_run test_send_string_full
mt_run test_send_string_too_big
mt_run test_send_bin
mt_run test_send_bin_full
mt_run test_send_bin_too_big
mt_run test_send_and_timeout
mt_run test_threaded
mt_run test_totally_random

mt_return

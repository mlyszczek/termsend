#!/usr/bin/env bash

updir="./kurload-test/out"
data="./kurload-test/data"
pidfile="./kurload-test/kurload.pid"

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
    tailn="tail -n2"
else
    nc="nc"
    tailn="tail -n1"
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


start_kurload()
{
    args="${1}" # additional arguments like "-U" for timed uploads

    mkdir -p ./kurload-test/out
    ../src/kurload -D -l7 -c -i61337 -s1024 -t3 -m3 -dlocalhost -ukurload \
        -gkurload -P"${pidfile}" \
        -q./kurload-test/kurload-query.log -p./kurload-test/kurload.log \
        -L./kurload-test/blacklist -T-1 -o./kurload-test/out \
        -b${server} ${args}
}


## ==========================================================================
## ==========================================================================


mt_prepare_test()
{
    start_kurload
}


## ==========================================================================
## ==========================================================================


mt_cleanup_test()
{
    pid="$(cat "${pidfile}")"
    kill -15 "${pid}"

    tries=0
    echo "killing kurload" >> /tmp/kurload
    while true
    do
        if ! kill -s 0 "${pid}" 2>/dev/null
        then
            # kurload died
            echo "it died" >> /tmp/kurload

            break
        fi

        tries=$(( tries + 1))

        if [ ${tries} -eq 5 ]
        then
        echo "could kill 5 times" >> /tmp/kurload
            break
        fi
        sleep 1
    done

    rm -rf ./kurload-test
}


## ==========================================================================
#   Parses output from kurload server to get generated file name where data
#   was stored in
## ==========================================================================


get_file()
{
    tail -n1 | rev | cut -d/ -f-1 | rev
}


## ==========================================================================
#   sends content of file from path $1, to kurload server and returns
#   response from the server. Tries for up to 5 seconds
## ==========================================================================


kurload()
{
    file="${1}"
    append_kurload="${2}"

    if [ -z "${append_kurload}" ]
    then
        append_kurload=1
    fi

    echo "sending file $1 append $2" >> /tmp/kurload
    tries=0
    while true
    do
        printf "" > "${file}.ncerr"

        if [ ${append_kurload} -eq 1 ]
        then
            out="$(cat "${file}" | { cat -; echo 'kurload'; } | \
                ${nc} -v ${server} 61337 2>"${file}.ncerr")"
        else
            out="$(cat "${file}" | ${nc} -v ${server} 61337 \
                2>"${file}.ncerr")"
        fi

        if grep -i "open" "${file}.ncerr" || grep -i "succe" "${file}.ncerr"
        then
            # nc was successfull
            echo "nc allright" >> /tmp/kurload
            echo $out >> /tmp/kurload

            echo "${out}"
            return 0
        fi

        if [ ${tries} -eq 5 ]
        then
            # 5 seconds passed, server did not start, something
            # is wrong, abort, abort

            echo "nc fucked 5 times" >> /tmp/kurload
            return 1
        fi

        # nc failed, probably connection refused, let's try again

        echo "nc fucked" >> /tmp/kurload
        cat ${file}.ncerr >> /tmp/kurload
        tries=$(( tries + 1 ))
        sleep 1
    done
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
    out="$(kurload "${data}.${fname}" | tail -n1)"

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
    randstr 128 > ${data}
    file="$(kurload "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


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
    randstr 1023 > ${data}
    file="$(kurload "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_string_too_big()
{
    randstr 1337 > ${data}
    out="$(kurload "${data}" | tail -n1)"
    mt_fail "[ \"${out}\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_bin()
{
    randbin 128 > ${data}
    file="$(kurload "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_bin_full()
{
    randbin 1024 > ${data}
    file="$(kurload ${data} | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_bin_too_big()
{
    randbin 1337 > "${data}"
    out="$(kurload "${data}" | tail -n1)"
    mt_fail "[ \"${out}\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_and_timeout()
{
    randbin 128 > "${data}"
    out="$(kurload "${data}" 0 | ${tailn} | tr "\n" ".")"
    mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
}


## ==========================================================================
## ==========================================================================


test_timed_upload()
{
    ###
    # start kurload with -U (timed upload) flag set
    #

    start_kurload "-U"
    randbin 128 > "${data}"
    file="$(kurload "${data}" 0 | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_full()
{
    start_kurload "-U"
    randbin 1024 > ${data}
    file="$(kurload "${data}" 0 | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_too_big()
{
    start_kurload "-U"
    randbin 1337 > ${data}
    out="$(kurload "${data}" 0 | tail -n1)"
    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_with_kurload()
{
    start_kurload "-U"
    randbin 128 > "${data}"
    file="$(kurload "${data}" | get_file)"
    mt_fail "diff $updir/$file $data"
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
            out="$(kurload ${data} 0 | ${tailn} | tr "\n" ".")"

            mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
        else
            randbin $numbytes > $data

            if [ $numbytes -gt 1024 ]
            then
                out="$(kurload "${data}" | tail -n1)"
                if [ ! -z "$out" ]
                then
                    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
                fi
            else
                file="$(kurload "${data}" | get_file)"
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

###
# these tests have custom preparation code so remove default preparation
# function and define an empty one
#

unset mt_prepare_test
mt_prepare_test()
{
    nop=1
}

###
# now run tests without preparation function called
#

mt_run test_timed_upload
mt_run test_timed_upload_full
mt_run test_timed_upload_too_big
mt_run test_timed_upload_with_kurload

mt_return

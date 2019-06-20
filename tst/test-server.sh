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

have_nc=1
if [ "${os}" = "SunOS" ]
then
    nc="nc -F"
    tailn="tail -n2"
else
    nc="nc"
    tailn="tail -n1"
fi

have_socat_openssl=0
if type socat > /dev/null
then
    have_socat_openssl=1
    socat_openssl="socat -t30 - OPENSSL:${server}:61338,verify=0"
fi

## ==========================================================================
#                  _                __           ____
#    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
#   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
#  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
# / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
#/_/
## ==========================================================================


start_kurload()
{
    args="${1}" # additional arguments like "-U" for timed uploads

    mkdir -p ./kurload-test/out
    if [ ${ssl_test} = "openssl" ]
    then
        ../src/kurload -D -l7 -c -i61337 -s1024 -t3 -m3 -dlocalhost -ukurload \
            -gkurload -P"${pidfile}" -I61338 -k./test-server.key.pem \
            -C./test-server.cert.pem -f./test-server.key.pass \
            -q./kurload-test/kurload-query.log -p./kurload-test/kurload.log \
            -L./kurload-test/blacklist -T-1 -o./kurload-test/out \
            -b${server} ${args}
    else
        ../src/kurload -D -l7 -c -i61337 -s1024 -t3 -m3 -dlocalhost -ukurload \
            -gkurload -P"${pidfile}" \
            -q./kurload-test/kurload-query.log -p./kurload-test/kurload.log \
            -L./kurload-test/blacklist -T-1 -o./kurload-test/out \
            -b${server} ${args}
    fi
}


## ==========================================================================
## ==========================================================================


mt_prepare_test()
{
    if [ ${prepare_test_on} -eq 1 ]
    then
        start_kurload
    fi
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
            exit 1
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


kurload_nc()
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


kurload_socat_openssl()
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
        if [ ${append_kurload} -eq 1 ]
        then
            out="$(cat "${file}" | { cat -; echo 'kurload'; } | \
                ${socat_openssl})"
        else
            out="$(cat "${file}" | ${socat_openssl})"
        fi

        if [ ${?} -eq 0 ]
        then
            # socat was successfull
            echo "socat openssl allright" >> /tmp/kurload
            echo $out >> /tmp/kurload

            echo "${out}"
            return 0
        fi

        if [ ${tries} -eq 5 ]
        then
            # 5 seconds passed, server did not start, something
            # is wrong, abort, abort

            echo "socat openssl fucked 5 times" >> /tmp/kurload
            return 1
        fi

        # socat failed, probably connection refused, let's try again

        echo "socat openssl fucked" >> /tmp/kurload
        tries=$(( tries + 1 ))
        sleep 1
    done
}


kurload()
{
    case ${prog_test}-${ssl_test} in
    nc-none)
        kurload_nc ${@}
        ;;

    socat-openssl)
        kurload_socat_openssl ${@}
        ;;

    *)
        echo "invalid kurload arguments: ${ssl_test}${have_nc}"
        ;;
    esac
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

    # sleep for 1 second, since it is possible that we run the test
    # so quickly, that in cleanup kill SIGTERM is emited to early
    # and test fails. This is especially true when using compiler
    # sanitizers.

    sleep 1
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
    if [ "${prog_test}" = "socat" ]
    then
        mt_fail "[[ \"$out\" == \"FIN received but not ending "kurload\n" string is present - discarding.\"* ]]"
    else
        mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
    fi
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

            if [ "${prog_test}" = "socat" ]
            then
                mt_fail "[[ \"$out\" == \"FIN received but not ending "kurload\n" string is present - discarding.\"* ]]"
            else
                mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
            fi

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
## ==========================================================================


check_sanitizer_logs()
{
    if grep "==ERROR: " ${0}.log
    then
        # succes means ERROR appears
        mt_fail "[ \"sanitizer log contains ERROR\" = \"error force\" ]"
    fi
}


## ==========================================================================
#   __               __                                    __   _
#  / /_ ___   _____ / /_   ___   _  __ ___   _____ __  __ / /_ (_)____   ____
# / __// _ \ / ___// __/  / _ \ | |/_// _ \ / ___// / / // __// // __ \ / __ \
#/ /_ /  __/(__  )/ /_   /  __/_>  < /  __// /__ / /_/ // /_ / // /_/ // / / /
#\__/ \___//____/ \__/   \___//_/|_| \___/ \___/ \__,_/ \__//_/ \____//_/ /_/
#
## ==========================================================================


run_tests()
{
    prepare_test_on=1

    mt_run_named test_is_running "test_is_running-${prog_test}-${ssl_test}"
    mt_run_named test_send_string "test_send_string-${prog_test}-${ssl_test}"
    mt_run_named test_send_string_full "test_send_string_full-${prog_test}-${ssl_test}"
    mt_run_named test_send_string_too_big "test_send_string_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin "test_send_bin-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin_full "test_send_bin_full-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin_too_big "test_send_bin_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_send_and_timeout "test_send_and_timeout-${prog_test}-${ssl_test}"
    mt_run_named test_threaded "test_threaded-${prog_test}-${ssl_test}"
    mt_run_named test_totally_random "test_totally_random-${prog_test}-${ssl_test}"

    # these tests have custom preparation code so remove default preparation
    # function and define an empty one

    prepare_test_on=0

    # now run tests without preparation function called

    mt_run_named test_timed_upload "test_timed_upload-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_full "test_timed_upload_full-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_too_big "test_timed_upload_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_with_kurload "test_timed_upload_with_kurload-${prog_test}-${ssl_test}"
}

rm -rf ./kurload-test

ssl_test=none
prog_test=nc
run_tests

../src/kurload -h | grep listen-ssl-port > /dev/null
have_ssl=${?}

if [ ${have_socat_openssl} -eq 1 ] && [ ${have_ssl} -eq 0 ]
then
    ssl_test=openssl
    prog_test=socat
    run_tests
fi

# last but not least, when running sanitizer tests, our test suite
# will succed even if sanitizer reports memory leak or write out of
# bound, because usually such errors does not crash app but are
# rather exploitable security issues. So we grep log file for
# sanitizer errors and test fail when there was any error.

mt_run check_sanitizer_logs

mt_return

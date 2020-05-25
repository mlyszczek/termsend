#!/usr/bin/env bash

updir="./termsend-test/out"
data="./termsend-test/data"
pidfile="$(pwd)/termsend-test/termsend.pid"
g_args=""

. ./mtest.sh
os="$(uname)"
if [ "${os}" = "Linux" ]
then
    server="127.$((RANDOM % 256)).$((RANDOM % 256)).$((RANDOM % 254 + 2))"
else
    server="127.0.0.1"
fi

## ==========================================================================
#                  _                __           ____
#    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
#   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
#  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
# / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
#/_/
## ==========================================================================


start_termsend()
{
    common_opts="-D -l7 -c -i61337 -a61338 -s1024 -t3 -m8 -dlocalhost -utermsend \
        -gtermsend -P"${pidfile}" -q`pwd`/termsend-test/termsend-query.log \
        -p`pwd`/termsend-test/termsend.log -T0 \
        -o`pwd`/termsend-test/out -b${server}"

    mkdir -p ./termsend-test/out
    if [ ${ssl_test} = "openssl" ]
    then
        ../src/termsend ${common_opts} -I61339 -A61340 -k./test-server.key.pem \
            -C./test-server.cert.pem -f./test-server.key.pass ${g_args}
    else
        ../src/termsend ${common_opts} ${g_args}
    fi

    # wait for termsend to start

    tries=0
    while true
    do
        if grep "n/server initialized and started" ./termsend-test/termsend.log \
            >/dev/null
        then
            break
        fi

        tries=$(( tries + 1 ))
        if [ ${tries} -eq 600 ]
        then
            echo "termsend failed to start" >> /tmp/termsend
            exit 1
        fi

        sleep 0.1
    done
}


## ==========================================================================
## ==========================================================================


mt_prepare_test()
{
    start_termsend
}


## ==========================================================================
## ==========================================================================


mt_cleanup_test()
{
    pid="$(cat "${pidfile}")"
    kill -15 "${pid}"

    tries=0
    echo "killing termsend" >> /tmp/termsend
    while true
    do
        if ! kill -s 0 "${pid}" 2>/dev/null
        then
            # termsend died
            echo "it died" >> /tmp/termsend

            break
        fi

        tries=$(( tries + 1))

        if [ ${tries} -eq 5 ]
        then
        echo "could kill 5 times" >> /tmp/termsend
            exit 1
            break
        fi
        sleep 1
    done

    rm -rf ./termsend-test
}


## ==========================================================================
#   Parses output from termsend server to get generated file name where data
#   was stored in
## ==========================================================================


get_file()
{
    tail -n1 | rev | cut -d/ -f-1 | rev
}


## ==========================================================================
#   sends content of file from path $1, to termsend server and returns
#   response from the server. Tries for up to 5 seconds
## ==========================================================================


termsend_nc()
{
    port="${1}"
    file="${2}"
    append_termsend="${3}"

    if [ -z "${append_termsend}" ]
    then
        append_termsend=1
    fi

    echo "sending file $1 append $2" >> /tmp/termsend
    tries=0
    while true
    do
        printf "" > "${file}.ncerr"

        if [ ${append_termsend} -eq 1 ]
        then
            out="$(cat "${file}" | { cat -; echo 'termsend'; } | \
                ${nc} -v ${server} ${port} 2>"${file}.ncerr")"
        else
            out="$(cat "${file}" | ${nc} -v ${server} ${port} \
                2>"${file}.ncerr")"
        fi

        if egrep -i "open|succe|received" "${file}.ncerr"
        then
            # nc was successfull
            echo "nc allright" >> /tmp/termsend
            echo $out >> /tmp/termsend

            echo "${out}"
            return 0
        fi

        if [ ${tries} -eq 5 ]
        then
            # 5 seconds passed, server did not start, something
            # is wrong, abort, abort

            echo "nc fucked 5 times" >> /tmp/termsend
            return 1
        fi

        # nc failed, probably connection refused, let's try again

        echo "nc fucked" >> /tmp/termsend
        cat ${file}.ncerr >> /tmp/termsend
        tries=$(( tries + 1 ))
        sleep 1
    done
}


termsend_socat()
{
    port="${1}"
    file="${2}"
    append_termsend="${3}"

    if [ -z "${append_termsend}" ]
    then
        append_termsend=1
    fi

    if [ "${ssl_test}" = "openssl" ]
    then
        socat_cmd="socat -t30 - OPENSSL:${server}:${port},verify=0"
    else
        socat_cmd="socat -t30 - TCP:${server}:${port}"
    fi

    echo "sending file $1 append $2" >> /tmp/termsend
    tries=0
    while true
    do
        if [ ${append_termsend} -eq 1 ]
        then
            out="$(cat "${file}" | { cat -; echo 'termsend'; } | \
                ${socat_cmd})"
        else
            out="$(cat "${file}" | ${socat_cmd})"
        fi

        if [ ${?} -eq 0 ]
        then
            # socat was successfull
            echo "socat allright" >> /tmp/termsend
            echo $out >> /tmp/termsend

            echo "${out}"
            return 0
        fi

        if [ ${tries} -eq 5 ]
        then
            # 5 seconds passed, server did not start, something
            # is wrong, abort, abort

            echo "socat fucked 5 times" >> /tmp/termsend
            return 1
        fi

        # socat failed, probably connection refused, let's try again

        echo "socat fucked" >> /tmp/termsend
        tries=$(( tries + 1 ))
        sleep 1
    done
}


termsend()
{
    case ${prog_test}-${ssl_test} in
    nc-none)
        port=61337
        if [ ${timed_test} -eq 1 ]; then port=61338; fi

        termsend_nc ${port} ${@}
        ;;

    socat-none)
        port=61337
        if [ ${timed_test} -eq 1 ]; then port=61338; fi

        termsend_socat ${port} ${@}
        ;;

    socat-openssl)
        port=61339
        if [ ${timed_test} -eq 1 ]; then port=61340; fi

        termsend_socat ${port} ${@}
        ;;

    *)
        echo "invalid termsend arguments: ${ssl_test}${have_nc}"
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
    out="$(termsend "${data}.${fname}" | tail -n1)"

    if [ "${out}" = "all upload slots are taken, try again later" ]
    then
        touch "${1}.test_check"
        return 0
    elif [[ "${out}" = "localhost/"* ]]
    then
        file="$(echo "${out}" | rev | cut -d/ -f-1 | rev)"
        mt_fail "diff ${updir}/${file} ${data}.${fname}"
        touch "${1}.test_check"
        return $?
    else
        if [ -z "${out}" ]
        then
            touch "${1}.test_check"
            return
        fi

        echo "something weird received: '${out}'"
        mt_fail false
        touch "${1}.test_check"
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
    mt_fail "kill -s 0 `cat ${pidfile}`"

    # sleep for 1 second, since it is possible that we run the test
    # so quickly, that in cleanup kill SIGTERM is emited to early
    # and test fails. This is especially true when using compiler
    # sanitizers.

    sleep 1
}

## ==========================================================================
## ==========================================================================


test_mime_text_c()
{
    if echo "${g_args}" | grep "\-F"
    then
        # ft-based-url enabled
        out="$(termsend main.c | tail -n1)"
        mime="$(echo ${out} | rev | cut -d/ -f2 | rev)"
        file="$(echo ${out} | rev | cut -d/ -f-1 | rev)"

        mt_fail "[ \"x${mime}\" = \"xx-c\" ]"
    else
        file="$(termsend main.c | get_file)"
    fi

    mt_fail "diff ${updir}/${file} main.c"
}


## ==========================================================================
## ==========================================================================


test_mime_text_shellscript()
{
    if echo "${g_args}" | grep "\--ft-based-urll"
    then
        # ft-based-url enabled
        out="$(termsend shell-test.sh | tail -n1)"
        mime="$(echo ${out} | rev | cut -d/ -f2 | rev)"
        file="$(echo ${out} | rev | cut -d/ -f-1 | rev)"

        mt_fail "[ \"x${mime}\" = \"xx-shellscript\" ]"
    else
        file="$(termsend shell-test.sh | get_file)"
    fi

    mt_fail "diff ${updir}/${file} shell-test.sh"
}


## ==========================================================================
## ==========================================================================


test_mime_text_plain()
{
    file="$(termsend test-server.key.pass | get_file)"
    mt_fail "diff ${updir}/${file} test-server.key.pass"
}


## ==========================================================================
## ==========================================================================


test_mime_bin()
{
    printf "a\0\0\0s\0\0\0" > binfile.tmp
    file="$(termsend binfile.tmp | get_file)"
    mt_fail "diff ${updir}/${file} binfile.tmp"
    unlink binfile.tmp
}


## ==========================================================================
## ==========================================================================


test_send_string()
{
    randstr 128 > ${data}
    file="$(termsend "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_threaded()
{
    max_wait=60

    for i in $(seq 1 1 16)
    do
        multi_thread_check ${i} &
    done

    # we expect 16 files to be created, finish test once all 16 files
    # are present

    i=0
    while true
    do
        num_files="$(ls -1 *.test_check | wc -l)"
        if [ ${num_files} -eq 16 ]
        then
            # all files generated, we're good
            break
        fi

        if [ ${i} -gt ${max_wait} ]
        then
            mt_fail "[ \"threaded test timedout\" = \"error force\" ]"
            break
        fi

        i=$(( i + 1 ))
        sleep 1
    done

    rm *.test_check
}

## ==========================================================================
## ==========================================================================


test_send_string_full()
{
    randstr 1023 > ${data}
    file="$(termsend "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_string_too_big()
{
    randstr 1337 > ${data}
    out="$(termsend "${data}" | tail -n1)"
    mt_fail "[ \"${out}\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_bin()
{
    randbin 128 > ${data}
    file="$(termsend "${data}" | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_bin_full()
{
    randbin 1024 > ${data}
    file="$(termsend ${data} | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_send_bin_too_big()
{
    randbin 1337 > "${data}"
    out="$(termsend "${data}" | tail -n1)"
    mt_fail "[ \"${out}\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_empty_with_termsend()
{
    truncate -s0 "${data}"
    out="$(termsend "${data}" | tail -n1)"
    mt_fail "[ \"${out}\" == \"no data has been sent\" ]"
}


## ==========================================================================
## ==========================================================================


test_send_empty()
{
    truncate -s0 "${data}"
    out="$(termsend "${data}" 0 | ${tailn} | tr "\n" ".")"

    if [ "${prog_test}" = "socat" ] || \
        [ "${prog_test}" = "nc" -a "${nctype}" = "nmap" ]
    then
        mt_fail "[ \"${out}\" == \"no data has been sent.\" ]"
    else
        mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
    fi

}


## ==========================================================================
## ==========================================================================


test_send_without_termsend()
{
    randbin 128 > "${data}"
    if [ "${prog_test}" = "socat" ] || \
        [ "${prog_test}" = "nc" -a "${nctype}" = "nmap" ]
    then
        # socat and nmap version of nc nicely close connection when
        # all data is sent, so error will not happen here
        file="$(termsend ${data} | get_file)"
        mt_fail "diff ${updir}/${file} ${data}"
    else
        out="$(termsend "${data}" 0 | ${tailn} | tr "\n" ".")"
        mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
    fi
}


## ==========================================================================
## ==========================================================================


test_timed_upload()
{
    randbin 128 > "${data}"
    file="$(termsend "${data}" 0 | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_empty()
{
    truncate -s0 "${data}"
    out="$(termsend "${data}" 0 | ${tailn} | tr "\n" ".")"
    mt_fail "echo \"${out}\" | grep \"no data has been sent.\""
}


## ==========================================================================
## ==========================================================================


test_timed_upload_full()
{
    randbin 1024 > ${data}
    file="$(termsend "${data}" 0 | get_file)"
    mt_fail "diff ${updir}/${file} ${data}"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_too_big()
{
    randbin 1337 > ${data}
    out="$(termsend "${data}" 0 | tail -n1)"
    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
}


## ==========================================================================
## ==========================================================================


test_timed_upload_with_termsend()
{
    randbin 128 > "${data}"
    file="$(termsend "${data}" | get_file)"
    mt_fail "diff $updir/$file $data"
}


## ==========================================================================
## ==========================================================================


test_totally_random()
{
    for i in `seq 1 1 128`
    do
        numbytes=$(((RANDOM % 2048) + 1))
        finish=$((RANDOM % 32))


        if [ $finish -eq 0 ]
        then
            # send data but don't send ending termsend\n
            if [ $numbytes -gt 1024 ]
            then
                numbytes=512
            fi

            randbin $numbytes > $data

            if [ "${prog_test}" = "socat" ] || \
                [ "${prog_test}" = "nc" -a "${nctype}" = "nmap" ]
            then
                # socat and nmap version of nc nicely close connection when
                # all data is sent, so error will not happen here
                file="$(termsend ${data} | get_file)"
                mt_fail "diff ${updir}/${file} ${data}"
            else
                out="$(termsend ${data} 0 | ${tailn} | tr "\n" ".")"
                mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"
            fi

        else
            randbin $numbytes > $data

            if [ $numbytes -gt 1024 ]
            then
                out="$(termsend "${data}" | tail -n1)"
                if [ ! -z "$out" ]
                then
                    mt_fail "[ \"$out\" == \"file too big, max length is 1024 bytes\" ]"
                fi
            else
                file="$(termsend "${data}" | get_file)"
                mt_fail "diff $updir/$file $data"
            fi
        fi

    done
}


## ==========================================================================
## ==========================================================================


run_with_timeout()
{
    truncate -s0 "${data}"
    start=$(date +%s%N)
    start=$((start / 1000000))
    out="$(termsend "${data}" 0 | ${tailn} | tr "\n" ".")"

    mt_fail "[[ \"$out\" == \"disconnected due to inactivity for 3 seconds, did you forget to append termination string\"* ]]"

    #echo $out
    if [ ${mt_test_status} -eq 1 ]
    then
        return 1
    fi

    now=$(date +%s%N)
    oper_time=$((now/1000000 - start))
    mt_fail "[ ${oper_time} -gt 2800 ] && [ ${oper_time} -lt 3200 ]"
    if [ ${mt_test_status} -eq 1 ]
    then
        # we expect for timeout error to be received after 3 seconds
        # but since world is not perfect we accept no more than 200[ms]
        # slide. This test checks if we properly calculate SIGALRM
        # and and really disconnect clients after 3 seconds, and not
        # let's say 10. On system that is heavy loaded, this may
        # rise false positive alarm as it may take longer than 3.2s
        # to handle client

        return 1
    fi

    touch ${1}.test_check
}

test_mixed_timeouts()
{
    i=0
    for i in $(seq 1 1 15)
    do
        if [ $(( i % 3 )) ]
        then
            # just to add some madness to the test
            test_send_string &
        fi

        run_with_timeout ${i} &
        sleep 0.1

        if [ $(( i % 2 )) -eq 0 ]
        then
            sleep 1
        fi
    done


    sleep 4

    num_files="$(ls -1 *.test_check | wc -l)"
    mt_fail "[ ${num_files} -eq 15 ]"
    rm *.test_check
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
#           _         ____                        __   __
#          (_)____   / __/____     ____ _ ____ _ / /_ / /_   ___   _____
#         / // __ \ / /_ / __ \   / __ `// __ `// __// __ \ / _ \ / ___/
#        / // / / // __// /_/ /  / /_/ // /_/ // /_ / / / //  __// /
#       /_//_/ /_//_/   \____/   \__, / \__,_/ \__//_/ /_/ \___//_/
#                               /____/
## ==========================================================================


have_nc=1
if [ "${os}" = "SunOS" ]
then
    nc="nc -F"
    tailn="tail -n2"
else
    nc="nc"
    tailn="tail -n1"
fi

if nc -v 2>&1 | grep "nmap.org" > /dev/null
then
    nctype=nmap
else
    nctype=hobbit
fi

have_socat=0
if type socat > /dev/null
then
    have_socat=1
fi

have_ssl=0
if ../src/termsend -v | grep +ssl > /dev/null
then
    have_ssl=1
fi

echo > /tmp/termsend

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
    timed_test=0

    mt_run_named test_is_running "test_is_running-${prog_test}-${ssl_test}"
    mt_run_named test_send_string "test_send_string-${prog_test}-${ssl_test}"
    mt_run_named test_send_string_full "test_send_string_full-${prog_test}-${ssl_test}"
    mt_run_named test_send_string_too_big "test_send_string_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin "test_send_bin-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin_full "test_send_bin_full-${prog_test}-${ssl_test}"
    mt_run_named test_send_bin_too_big "test_send_bin_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_send_without_termsend "test_send_without_termsend-${prog_test}-${ssl_test}"
    mt_run_named test_send_empty_with_termsend "test_send_empty_with_termsend-${prog_test}-${ssl_test}"
    mt_run_named test_send_empty "test_send_empty-${prog_test}-${ssl_test}"
    mt_run_named test_threaded "test_threaded-${prog_test}-${ssl_test}"
    mt_run_named test_totally_random "test_totally_random-${prog_test}-${ssl_test}"

    # run mime test with ft-based-url disabled
    mt_run_named test_mime_text_c "test_mime_text_c-${prog_test}-${ssl_test}"
    mt_run_named test_mime_text_shellscript "test_mime_text_shellscript-${prog_test}-${ssl_test}"
    mt_run_named test_mime_text_plain "test_mime_text_plain-${prog_test}-${ssl_test}"
    mt_run_named test_mime_bin "test_mime_bin-${prog_test}-${ssl_test}"

    # run mime test with ft-based-url enabled
    g_args="--ft-based-url"
    mt_run_named test_mime_text_c "test_mime_text_c-${prog_test}-${ssl_test}"
    mt_run_named test_mime_text_shellscript "test_mime_text_shellscript-${prog_test}-${ssl_test}"
    mt_run_named test_mime_text_plain "test_mime_text_plain-${prog_test}-${ssl_test}"
    mt_run_named test_mime_bin "test_mime_bin-${prog_test}-${ssl_test}"
    g_args=""

    timed_test=1

    mt_run_named test_timed_upload "test_timed_upload-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_empty "test_timed_upload_empty-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_full "test_timed_upload_full-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_too_big "test_timed_upload_too_big-${prog_test}-${ssl_test}"
    mt_run_named test_timed_upload_with_termsend "test_timed_upload_with_termsend-${prog_test}-${ssl_test}"
}

rm -rf ./termsend-test

ssl_test=none
prog_test=nc
run_tests

if [ ${have_socat} -eq 1 ]
then
    ssl_test=none
    prog_test=socat
    run_tests
fi

if [ ${have_socat} -eq 1 ] && [ ${have_ssl} -eq 1 ]
then
    ssl_test=openssl
    prog_test=socat
    run_tests
fi

if [ "x${optional_tests}" = "x1" ]
then
    # these tests are optional as they need precise environment and
    # can easily report false positives.

    ssl_test=none
    prog_test=nc
    timed_test=0

    mt_run_named test_mixed_timeouts "test_mixed_timeouts-nc-none"
fi

# last but not least, when running sanitizer tests, our test suite
# will succed even if sanitizer reports memory leak or write out of
# bound, because usually such errors does not crash app but are
# rather exploitable security issues. So we grep log file for
# sanitizer errors and test fail when there was any error.

unset mt_prepare_test
unset mt_cleanup_test
mt_run check_sanitizer_logs

mt_return

About
=====

Server application to share files (or program output) from command line using
the most basic UNIX tools, like **netcat**.

Usage
=====

Client
------

Clients doesn't need any sophisticated tools, to upload to server pipe standard
output from any application to **netcat**. But because **netcat** doesn't send
information about file size, you need to append "kurload\n" string at the end
of upload as and end-of-file indicator.

~~~
cat /path/to/file | cat - <(echo 'kurload') | nc kurwinet.pl 1337'
~~~

Quite long and irritating. It is recommended to create alias to work-around this
tedious work.

~~~
alias kl="cat - <(echo 'kurload') | nc kurwinet.pl 1337"
~~~

Now you can upload file as simple as calling

~~~
ls -l | kl
~~~

Note that process substitution **<()** only works with bash-compatible shell.

Server
------

~~~
$ kurload -h
kurload - easy file sharing

Usage: ./kurload [-h | -v | -d -c -l<level> -f<config>]

        -h         prints this help and quits
        -v         prints version and quits
        -d         run as daemon
        -c         if set, output will have nice colors
        -l<level>  logging level 0-7
        -f<path>   path to configuration file

logging level
        0          fatal errors, application cannot continue
        1          major failure, needs immediate attention
        2          critical errors
        3          error but recoverable
        4          warnings
        5          normal message, but of high importance
        6          info log, doesn't print that much (default)
        7          debug, not needed in production
~~~

For description of configuration options, refer to example configuration file
in src/kurload.conf

Dependencies
============

* libconfuse3 (https://github.com/martinh/libconfuse)
* embedlog1 (https://github.com/mlyszczek/embedlog)
* pthread

Compile and install
===================

Program uses autotools so instalation is as easy as

~~~
$ autoreconf -i
$ ./configure
$ make
# make install
~~~

Tests can be run with

~~~
$ make check
~~~

[kursg-meta]: # (order: 1)

About
=====

Server application to share files (or program output) from command line using
the most basic UNIX tools, like **netcat**.

Usage
=====

Client
------

Clients doesn't need any sophisticated tools. To upload to server pipe standard
output from any application to **netcat**. But because **netcat** doesn't send
information about file size, you need to append **kurload\n** string at the end
of upload as and end-of-file indicator.

~~~
$ cat /path/to/file | { cat -; echo 'kurload'; } | nc kl.kurwinet.pl 1337
~~~

Quite long and irritating to type everytime you want to upload something. It is
recommended to create alias to work-around this tedious work.

~~~
alias kl="{ cat -; echo 'kurload'; } | nc kl.kurwinet.pl 1337"
~~~

Now you can upload file as simple as calling

~~~
$ ls -l | kl               # uploads detailed list of files in current directory
$ cat error.log | kl       # uploads file 'error.log'
$ make | kl                # uploads compilation output
$ cat binary-file | kl     # uploads some binary file
~~~

After transfer is complete, server will print link which you can later use to
get uploaded content (like send it to someone via IRC). If uploaded content is
a simple text file, you can read it directly in terminal using **curl**, or if
output is known to be big, **curl** output can be piped to **less**. Check out
this simple example.

~~~
$ make distcheck 2>&1 | kl
nc: using stream socket
uploaded       4100 bytes
uploaded       5351 bytes
uploaded       8381 bytes
uploaded      16044 bytes
uploaded      29463 byte
upload complete, link to file http://kl.kurwinet.pl/msf62
$ curl http://kl.kurwinet.pl/msf62 | less
~~~

In this example, we upload output of **make distcheck** program into server, and
later we read in in less (for example on another computer).

Server will notify uploader about how much bytes were transfered every second.
If information is no received for longer than 1 second, that means program did
not produce any output and server didn't receive any data.

Server
------

Information about server usage and its options can be found in man page
[kurload](http://kurload.kurwinet.pl/kurload.1.html)(1)

Test results
============

operating system tests
----------------------

* parisc-polarhome-hpux-11.11 ![test-result-svg][prhpux]
* power4-polarhome-aix-7.1 ![test-result-svg][p4aix]
* i686-builder-freebsd-11.1 ![test-result-svg][x32fb]
* i686-builder-netbsd-8.0 ![test-result-svg][x32nb]
* i686-builder-openbsd-6.2 ![test-result-svg][x32ob]
* x86_64-builder-solaris-11.3 ![test-result-svg][x64ss]
* i686-builder-linux-gnu-4.9 ![test-result-svg][x32lg]
* i686-builder-linux-musl-4.9 ![test-result-svg][x32lm]
* i686-builder-linux-uclibc-4.9 ![test-result-svg][x32lu]
* x86_64-builder-linux-gnu-4.9 ![test-result-svg][x64lg]
* x86_64-builder-linux-musl-4.9 ![test-result-svg][x64lm]
* x86_64-builder-linux-uclibc-4.9 ![test-result-svg][x64lu]

machine tests
-------------

* aarch64-builder-linux-gnu ![test-result-svg][a64lg]
* armv5te926-builder-linux-gnueabihf ![test-result-svg][armv5]
* armv6j1136-builder-linux-gnueabihf ![test-result-svg][armv6]
* armv7a15-builder-linux-gnueabihf ![test-result-svg][armv7a15]
* armv7a9-builder-linux-gnueabihf ![test-result-svg][armv7a9]
* mips-builder-linux-gnu ![test-result-svg][m32lg]

sanitizers
----------

* -fsanitize=address ![test-result-svg][fsan]
* -fsanitize=leak ![test-result-svg][fsleak]
* -fsanitize=undefined ![test-result-svg][fsun]
* -fsanitize=thread ![test-result-svg][fsthread]

Dependencies
============

* [>=embedlog-0.5.0](http://embedlog.kurwinet.pl) (embedlog itself has no
  dependencies)
* pthread

Compile and install
===================

Program uses autotools so instalation is as easy as

~~~
$ ./autogen.sh
$ ./configure
$ make
# make install
~~~

License
=======

Program is licensed under BSD 2-clause license. See LICENSE file for details.

Contact
=======

Michał Łyszczek <michal.lyszczek@bofc.pl>

See also
========

* [embedlog](http://embedlog.kurwinet.pl) easy to use but feature-rich logger
  for **c/c++** applications
* [mtest](http://mtest.kurwinet.pl) macro unit test framework for **c/c++**
* [git repository](http://git.kurwinet.pl/kurload) to browse sources online
* [continous integration](http://ci.kurload.kurwinet.pl) with test results
* [polarhome](http://www.polarhome.com) nearly free shell accounts for virtually
  any unix there is.

[a64lg]: http://ci.kurload.kurwinet.pl/badges/aarch64-builder-linux-gnu-tests.svg
[armv5]: http://ci.kurload.kurwinet.pl/badges/armv5te926-builder-linux-gnueabihf-tests.svg
[armv6]: http://ci.kurload.kurwinet.pl/badges/armv6j1136-builder-linux-gnueabihf-tests.svg
[armv7a15]: http://ci.kurload.kurwinet.pl/badges/armv7a15-builder-linux-gnueabihf-tests.svg
[armv7a9]: http://ci.kurload.kurwinet.pl/badges/armv7a9-builder-linux-gnueabihf-tests.svg
[x32fb]: http://ci.kurload.kurwinet.pl/badges/i686-builder-freebsd-tests.svg
[x32lg]: http://ci.kurload.kurwinet.pl/badges/i686-builder-linux-gnu-tests.svg
[x32lm]: http://ci.kurload.kurwinet.pl/badges/i686-builder-linux-musl-tests.svg
[x32lu]: http://ci.kurload.kurwinet.pl/badges/i686-builder-linux-uclibc-tests.svg
[x32nb]: http://ci.kurload.kurwinet.pl/badges/i686-builder-netbsd-tests.svg
[x32ob]: http://ci.kurload.kurwinet.pl/badges/i686-builder-openbsd-tests.svg
[m32lg]: http://ci.kurload.kurwinet.pl/badges/mips-builder-linux-gnu-tests.svg
[x64lg]: http://ci.kurload.kurwinet.pl/badges/x86_64-builder-linux-gnu-tests.svg
[x64lm]: http://ci.kurload.kurwinet.pl/badges/x86_64-builder-linux-musl-tests.svg
[x64lu]: http://ci.kurload.kurwinet.pl/badges/x86_64-builder-linux-uclibc-tests.svg
[x64ss]: http://ci.kurload.kurwinet.pl/badges/x86_64-builder-solaris-tests.svg
[prhpux]: http://ci.kurload.kurwinet.pl/badges/parisc-polarhome-hpux-tests.svg
[p4aix]: http://ci.kurload.kurwinet.pl/badges/power4-polarhome-aix-tests.svg

[fsan]: http://ci.kurload.kurwinet.pl/badges/fsanitize-address.svg
[fsleak]: http://ci.kurload.kurwinet.pl/badges/fsanitize-leak.svg
[fsun]: http://ci.kurload.kurwinet.pl/badges/fsanitize-undefined.svg
[fsthread]: http://ci.kurload.kurwinet.pl/badges/fsanitize-thread.svg

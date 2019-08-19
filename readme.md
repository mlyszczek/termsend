[kursg-meta]: # (order: 1)

About
=====

Server application that allows you share files (or program output) from
command line using the most basic UNIX tools, like **netcat** or **socat**.

Usage
=====

Client
------

### uploading

Clients don't need any sophisticated tools. To upload to server - pipe standard
output from any application to **socat**:

Note: **kl.kurwinet.pl** is demo server which you can use to play with kurload.
You can also use it like ordinary no paste service to share data with people.

```
$ echo "test string" | socat - TCP:kl.kurwinet.pl:1337
```

Server reads data until **FIN** is seen or string **kurload\n** at the very
end of transfer. **socat** and nmap version of **netcat** send **FIN** when
stdin ends so it is advisible to use them. If you are stuck with hobbit
version of **netcat**, you need to also append **kurload\n** at the very end
of transfer:

```
$ echo "test string" | { cat -; echo 'kurload'; } | nc kl.kurwinet.pl 1337
```

If, for some reason, you are not able to pass **kurload\n**, you can always
use timed upload. In this mode, server will read data until there is no
activity on the socket for at least 3 seconds, after that **kurload** assumes
transfer to be complete and link is returned. This is not recommended, due to
the fact that you will have to wait 3 seconds after all data is sent, and you
might end up with incomplete upload when your output program stalls.

```
$ echo "test string" | nc kl.kurwinet.pl 1338
```

### easy to use alias

It's quite long and irritating to type these pipes everytime you want to
upload something. It is recommended to create alias to work-around this
tedious work.

```{.sh}
# add this to your .bashrc or .zshrc or whatever shell you use
alias kl="socat - TCP:kl.kurwinet.pl:1337"
```

Now you can upload anything by simply piping it to "kl" alias. Examples
will explain it best:

```
$ ls -l | kl               # uploads list of files in current directory
$ cat error.log | kl       # uploads file 'error.log'
$ make | kl                # uploads compilation output
$ cat binary-file | kl     # uploads some binary file
```

After transfer is complete, server will print link which you can later use to
get uploaded content (like send it to someone via IRC). If uploaded content is
a simple text file, you can read it directly in terminal using **curl**, or if
output is known to be big, **curl** output can be piped to **less**. Check out
this simple example.

```
$ make distcheck 2>&1 | kl
uploaded       3454 bytes
uploaded       8203 bytes
uploaded       9524 bytes
uploaded      11821 bytes
uploaded      16626 bytes
uploaded      23026 bytes
uploaded      31482 bytes
uploaded      32913 bytes
uploaded      33867 bytes
uploaded      40200 bytes
uploaded    1604104 bytes
uploaded    4668396 bytes
uploaded    4690455 bytes
upload complete, link to file https://kl.kurwinet.pl/o/6p3e1
$ curl https://kl.kurwinet.pl/o/6p3e1 | less
```

In this example, we upload output of **make distcheck** program into server, and
later we read in in less (for example on another computer).

Server will notify uploader about how much bytes were transfered every second.
If information is not received for longer than 1 second, that means program did
not produce any output and server didn't receive any data.

For all aliases check [alias page](https://kurload.kurwinet.pl/aliases.html).

Server
------

Information about server usage and its options can be found in man page
[kurload](https://kurload.kurwinet.pl/kurload.1.html)(1).

Test results
============

Newest *kurload* is tested against these operating systems and architectures.
Note that test results are taken from **master** branch, release version
**always** passes all these tests.

operating system tests
----------------------

* parisc-polarhome-hpux-11.11 ![test-result-svg][prhpux]
* power4-polarhome-aix-7.1 ![test-result-svg][p4aix]
* i686-builder-freebsd-11.1 ![test-result-svg][x32fb]
* i686-builder-netbsd-8.0 ![test-result-svg][x32nb]
* i686-builder-openbsd-6.2 ![test-result-svg][x32ob]
* x86_64-builder-dragonfly-5.0 ![test-result-svg][x64df]
* x86_64-builder-solaris-11.3 ![test-result-svg][x64ss]
* i686-builder-linux-gnu-4.9 ![test-result-svg][x32lg]
* i686-builder-linux-musl-4.9 ![test-result-svg][x32lm]
* i686-builder-linux-uclibc-4.9 ![test-result-svg][x32lu]
* x86_64-builder-linux-gnu-4.9 ![test-result-svg][x64lg]
* x86_64-builder-linux-musl-4.9 ![test-result-svg][x64lm]
* x86_64-builder-linux-uclibc-4.9 ![test-result-svg][x64lu]
* i686-builder-qnx-6.4.0 ![test-result-svg][x32qnx]

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

* [>=embedlog-0.5.0](https://embedlog.kurwinet.pl) (embedlog itself has no
  dependencies)
* pthread

Compile and install
===================

Program uses autotools so instalation is as easy as

```{.sh}
$ ./autogen.sh
$ ./configure
$ make
# make install
```

License
=======

Program is licensed under BSD 2-clause license. See LICENSE file for details.

Contact
=======

Michał Łyszczek <michal.lyszczek@bofc.pl>

See also
========

* [embedlog](https://embedlog.kurwinet.pl) easy to use but feature-rich logger
  for **c/c++** applications
* [mtest](https://mtest.kurwinet.pl) macro unit test framework for **c/c++**
* [git repository](https://git.kurwinet.pl/kurload) to browse sources online
* [continous integration](http://ci.kurload.kurwinet.pl) with test results
* [polarhome](http://www.polarhome.com) nearly free shell accounts for virtually
  any unix there is.
* [pvs studio](https://www.viva64.com/en/pvs-studio) static code analyzer with
  free licenses for open source projects

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
[x32qnx]: http://ci.kurload.kurwinet.pl/badges/i686-builder-qnx-tests.svg
[x64df]: http://ci.kurload.kurwinet.pl/badges/x86_64-builder-dragonfly-tests.svg

[fsan]: http://ci.kurload.kurwinet.pl/badges/fsanitize-address.svg
[fsleak]: http://ci.kurload.kurwinet.pl/badges/fsanitize-leak.svg
[fsun]: http://ci.kurload.kurwinet.pl/badges/fsanitize-undefined.svg
[fsthread]: http://ci.kurload.kurwinet.pl/badges/fsanitize-thread.svg

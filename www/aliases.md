[kursg-meta]: # (title: client aliases)

client aliases
==============

Using **kurload** from the client side is very easy, but writing each time,
host and in case of ssl server's certificate, might be cumbersome. For that
it is advisible to create very simple to use alias. For aliases to work,
data should be piped to them like this

```{.sh}
echo "test string" | kl
cat some-file | kl
program --that --outputs --data | kl
```

posix shell
===========

These are aliases that should work on every shell that follows posix standard.

socat
-----

Send data over unencrypted socket using **socat** program. **socat** is cool
program, as it sends **FIN** when **stdin** ends, so there is no need to
append **kurload\n** at the end of transfer.
```{.sh}
alias kl="socat - TCP:kl.kurwinet.pl:1337"
```

Send data over encrypted socket using **socat** program, use this if you do not
need to verify that you are really talking to **kl.kurwinet.pl** server and not
some man-in-the-middle.
```{.sh}
alias kls="socat - OPENSSL:kl.kurwinet.pl:1339,verify=0"
```

Send data over encrypted socket using **socat** program and verify that
**kl.kurwinet.pl** is really valid server and not man-in-the-middle. Note
that you should treat all data sent to **kl.kurwinet.pl** as public and
this example is usefull when you self host **kurload** and want to have really
secure socket.
```{.sh}
alias kls="socat - OPENSSL:kl.kurwinet.pl:1339,cafile=/usr/share/ca/kurload.crt"
```

netcat
------

**netcat** is more popular and might be on systemd where **socat** is not
available. But some **netcat** implementations do not send **FIN** when
all data is sent, so client must either send data to timed-socket or include
ending **kurload\n** string. Also **netcat** usually does not support ssl.

Note, that **netcat** program may be called **nc**, **ncat** or **netcat**.

Send data over unencrypted, timed socket. Note that after all data is sent,
you will need to wait 3 seconds (or time configured on server) for server
to notice timeout to receive link.
```{.sh}
alias kl="nc kl.kurwinet.pl 1338"
```

Send data over unencrypted socket. This is workaround for **netcat** not
sending **FIN** packet. This will work with every flavour of **netcat**.
```{.sh}
alias kl="{ cat -; echo 'kurload'; } | nc kl.kurwinet.pl 1337"
```

bash
====

**bash** specific aliases

/dev/tcp
--------

For when there is neither **socat** nor **nc** available, there almost always
is **bash** present.
```{.bash}
alias kl="{ exec 5<>/dev/tcp/kl.kurwinet.pl/1337; \
    { cat - >&5; echo 'kurload' >&5; }; cat <&5; }"
```
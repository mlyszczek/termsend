#!/bin/sh

project="kurload"
scp_server="pkgs@kurwik"
revision="1"

atexit()
{
    set +e

    /etc/init.d/kurload stop
    removepkg "${project}"
    # remove dependencies
    removepkg embedlog
    # manually remove kurload group and user as package uninstall won't do it
    userdel kurload
    groupdel kurload
    rmdir /var/lib/kurload
    rm /etc/kurload.conf

    if [ "x${1}" != "xno-exit" ]
    then
        exit ${retval}
    fi
}
if [ ${#} -ne 3 ]
then
    echo "usage: ${0} <version> <arch> <host_os>"
    echo ""
    echo "where"
    echo "    <version>        git tag version"
    echo "    <arch>           target architecture"
    echo "    <host_os>        target os (slackware-14.2 etc)"
    exit 1
fi

git_version="${1}"
arch="${2}"
host_os="${3}"
workdir="/tmp/${project}-${arch}-${git_version}"

trap atexit EXIT
set -e

rm -rf "${workdir}"
mkdir "${workdir}"
cd "${workdir}"

wget "https://git.kurwinet.pl/${project}/snapshot/${project}-${git_version}.tar.gz"
tar xf "${project}-${git_version}.tar.gz"
cd "${project}-${git_version}"

# kurload rc script sources config from /usr/local/etc, and if package is
# installed from package manager, config file is in /etc
sed -i 's@^\. /usr/local/etc/kurload.conf$@. /etc/kurload.conf@' init.d/kurload
# same for command path
sed -i 's@^command=/usr/local/bin/kurload$@command=/usr/bin/kurload@' init.d/kurload

# install deps
wget https://distfiles.kurwinet.pl/embedlog/${host_os}/${arch}/embedlog-0.5.0-${arch}-1.tgz
installpkg embedlog-0.5.0-${arch}-1.tgz

version="$(grep "AC_INIT(" "configure.ac" | cut -f3 -d\[ | cut -f1 -d\])"
./autogen.sh
CFLAGS="-I/usr/bofc/include" LDFLAGS="-L/usr/bofc/lib" ./configure \
    --prefix=/usr --sysconfdir=/etc --enable-openssl
LD_LIBRARY_PATH=/usr/bofc/lib make

mkdir "${workdir}/root"
mkdir "${workdir}/root/install"
DESTDIR="${workdir}/root" make install

[ -f "pkg/tgz/doinst.sh" ] && cp "pkg/tgz/doinst.sh" "${workdir}/root/install"
cd "${workdir}/root"
find usr/share/man \( -name *.1 \) | xargs gzip
makepkg -l y -c n "${workdir}/${project}-${version}-${arch}-${revision}.tgz"
installpkg "${workdir}/${project}-${version}-${arch}-${revision}.tgz"

if ldd $(which kurload) | grep "\/usr\/bofc"
then
    echo "test prog uses libs from manually installed /usr/bofc \
        instead of system path!"
    exit 1
fi

kurload -v
/etc/init.d/kurload start
/etc/init.d/kurload restart
/etc/init.d/kurload stop

atexit "no-exit"

# run test prog again, but now fail if there is no error, testprog
# should fail as there is no library in the system any more
set +e
retval=0
kurload -v && retval=1
getent passwd kurload && retval=2
getent group kurload && retval=3
[ -d /var/lib/kurload ] && retval=4

if [ ${retval} -ne 0 ]
then
    echo "failed with status ${retval}"
    exit ${retval}
fi

set -e
retval=1
if [ -n "${scp_server}" ]
then
    echo "copying data to ${scp_server}:${project}/${host_os}/${arch}"
    scp "${workdir}/${project}-${version}-${arch}-${revision}.tgz" \
        "${scp_server}:${project}/${host_os}/${arch}" || exit 1
fi

retval=0

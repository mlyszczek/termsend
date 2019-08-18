#!/bin/sh

scp_server="pkgs@kurwik"
project="kurload"
retval=1

atexit()
{
    set +e

    /etc/init.d/kurload stop
    pkill kurload
    dpkg --purge "${project}"
    # remove dependencies
    dpkg -r libembedlog-dev
    dpkg -r libembedlog0
    # manually remove kurload group and user as package uninstall won't do it
    userdel kurload
    groupdel kurload
    rm -rf /var/lib/kurload
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
    echo "where:"
    echo "    <version>         git tag version to build (without prefix v)"
    echo "    <arch>            target architecture"
    echo "    <host_os>         target os (debian9, debian8 etc)"
    echo ""
    echo "example"
    echo "      ${0} 1.0.0 i386 debian9"
    exit 1
fi

version="${1}"
arch="${2}"
host_os="${3}"

trap atexit EXIT
set -e

###
# preparing
#

rm -rf "/tmp/${project}-${version}"
mkdir "/tmp/${project}-${version}"

cd "/tmp/${project}-${version}"
git clone "https://git.kurwinet.pl/${project}"
cd "${project}"

git checkout "${version}" || exit 1

if [ ! -d "pkg/deb" ]
then
    echo "pkg/deb does not exist, cannot create debian pkg"
    exit 1
fi

# kurload rc script sources config from /usr/local/etc, and if package is
# installed from package manager, config file is in /etc
sed -i 's@^\. /usr/local/etc/kurload.conf$@. /etc/kurload.conf@' init.d/kurload
# same for command path
sed -i 's@^command=/usr/local/bin/kurload$@command=/usr/bin/kurload@' init.d/kurload

version="$(grep "AC_INIT(" "configure.ac" | cut -f3 -d\[ | cut -f1 -d\])"

echo "version ${version}"

###
# building package
#

codename="$(lsb_release -c | awk '{print $2}')"

cp -r "pkg/deb/" "debian"
sed -i "s/@{DATE}/$(date -R)/" "debian/changelog.template"
sed -i "s/@{VERSION}/${version}/" "debian/changelog.template"
sed -i "s/@{CODENAME}/${codename}/" "debian/changelog.template"

mv "debian/changelog.template" "debian/changelog"
mv "debian/control.template" "debian/control"

# install build dependencies
apt-get install -y libembedlog0 libembedlog-dev

#export CFLAGS="-I/usr/bofc/include"
#export LDFLAGS="-L/usr/bofc/lib"
# no need to run make check, we run extensive tests before creating
# package
DEB_BUILD_OPTIONS=nocheck debuild -us -uc -d

# unset so these don't pollute gcc when we build test program
unset CFLAGS
unset LDFLAGS
unset LD_LIBRARY_PATH

###
# verifying
#

cd ..

# debuild doesn't fail when lintial finds an error, so we need
# to check it manually, it doesn't take much time, so whatever

for d in *.deb
do
    echo "Running lintian on ${d}"
    lintian ${d}
done

dpkg -i --no-triggers "${project}_${version}_${arch}.deb"

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
if [ -n "${scp_server}" ]
then
    dbgsym_pkg="${project}-dbgsym_${version}_${arch}.deb"

    if [ ! -f "${dbgsym_pkg}" ]
    then
        # on some systems packages with debug symbols are created with
        # ddeb extension and not deb
        dbgsym_pkg="${project}-dbgsym_${version}_${arch}.ddeb"
    fi

    echo "copying data to ${scp_server}:${project}/${host_os}/${arch}"
    scp "${project}-dev_${version}_${arch}.deb" \
        "${dbgsym_pkg}" \
        "${project}_${version}_${arch}.deb" \
        "${project}_${version}.dsc" \
        "${project}_${version}.tar.xz" \
        "${project}_${version}_${arch}.build" \
        "${project}_${version}_${arch}.buildinfo" \
        "${project}_${version}_${arch}.changes" \
        "${scp_server}:${project}/${host_os}/${arch}" || exit 1

    retval=0
fi

retval=0

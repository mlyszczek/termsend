#!/bin/sh

scp_server="pkgs@kurwik"
project="termsend"
retval=1

atexit()
{
    set +e

    /etc/init.d/termsend stop
    pkill termsend
    rm /etc/init.d/termsend
    dpkg --purge "${project}"
    # remove dependencies
    dpkg -r libembedlog-dev
    dpkg -r libembedlog0
    # manually remove termsend group and user as package uninstall won't do it
    userdel termsend
    groupdel termsend
    rm -rf /var/lib/termsend
    rm /etc/termsend.conf

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
git clone "https://git.bofc.pl/${project}"
cd "${project}"

git checkout "${version}" || exit 1

if [ ! -d "pkg/deb" ]
then
    echo "pkg/deb does not exist, cannot create debian pkg"
    exit 1
fi

# termsend rc script sources config from /usr/local/etc, and if package is
# installed from package manager, config file is in /etc
sed -i 's@^\. /usr/local/etc/termsend.conf$@. /etc/termsend.conf@' init.d/termsend
# same for command path
sed -i 's@^command=/usr/local/bin/termsend$@command=/usr/bin/termsend@' init.d/termsend

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

export RUNLEVEL=3
dpkg -i --no-triggers "${project}_${version}_${arch}.deb"

if ldd $(which termsend) | grep "\/usr\/bofc"
then
    echo "test prog uses libs from manually installed /usr/bofc \
        instead of system path!"
    exit 1
fi

termsend -v
/etc/init.d/termsend start
/etc/init.d/termsend restart
/etc/init.d/termsend stop

atexit "no-exit"

# run test prog again, but now fail if there is no error, testprog
# should fail as there is no library in the system any more
set +e
retval=0
termsend -v && retval=1
getent passwd termsend && retval=2
getent group termsend && retval=3
[ -d /var/lib/termsend ] && retval=4

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

#!/bin/sh

project="termsend"
scp_server="pkgs@kurwik"
retval=1

atexit()
{
    set +e

    /etc/init.d/termsend stop

    if type zypper >/dev/null
    then
        zypper remove -y embedlog-devel
        zypper remove -y embedlog

        zypper remove -y "${project}"
    else
        yum remove -y embedlog-devel
        yum remove -y embedlog

        yum remove -y "${project}"
    fi

    userdel termsend
    groupdel termsend
    rmdir /var/lib/termsend

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
    echo "    <host_os>        target os (rhel-7, centos-7 etc)"
    exit 1
fi

git_version="${1}"
arch="${2}"
host_os="${3}"

trap atexit EXIT
set -e

cd "${HOME}/rpmbuild"
pkg_version="$(curl "https://git.bofc.pl/${project}/plain/configure.ac?h=${git_version}" | \
    grep "AC_INIT(" | cut -f3 -d\[ | cut -f1 -d\])"
wget "https://git.bofc.pl/${project}/snapshot/${project}-${git_version}.tar.gz" \
    -O "SOURCES/${project}-${pkg_version}.tar.gz"
wget "https://git.bofc.pl/${project}/plain/pkg/rpm/${project}.spec.template?h=${git_version}" \
    -O "SPECS/${project}-${pkg_version}.spec"
lt_version="$(curl "https://git.bofc.pl/${project}/plain/lib/Makefile.am?h=${git_version}" | \
    grep "${project}_la_LDFLAGS = -version-info" | cut -f4 -d\ )"

current="$(echo ${lt_version} | cut -f1 -d:)"
revision="$(echo ${lt_version} | cut -f2 -d:)"
age="$(echo ${lt_version} | cut -f3 -d:)"

lib_version="$(( current - age )).${age}.${revision}"
abi_version="$(( current - age ))"
rel_version="$(cat SPECS/${project}-${pkg_version}.spec | \
    grep "Release:" | awk '{print $2}')"

sed -i "s/@{VERSION}/${pkg_version}/" SPECS/${project}-${pkg_version}.spec
sed -i "s/@{GIT_VERSION}/${git_version}/" SPECS/${project}-${pkg_version}.spec
sed -i "s/@{LIB_VERSION}/${lib_version}/" SPECS/${project}-${pkg_version}.spec
sed -i "s/@{ABI_VERSION}/${abi_version}/" SPECS/${project}-${pkg_version}.spec

# install deps
if type zypper >/dev/null
then
    zypper install -y embedlog embedlog-devel
else
    yum install -y embedlog embedlog-devel
fi

if cat /etc/os-release | grep "openSUSE Leap"
then
    # opensuse doesn't generate debug symbols by defaul, check spec file
    # for comment
    sed -i 's/# __DEBUG_PACKAGE__/%debug_package/' \
        SPECS/${project}-${pkg_version}.spec
fi

rpmbuild -ba SPECS/${project}-${pkg_version}.spec

###
# verify
#

if type zypper >/dev/null
then
    # looks like we are dealing with opensuse

    zypper install -y --allow-unsigned-rpm \
        "RPMS/${arch}/${project}-${pkg_version}-${rel_version}.${arch}.rpm"
else
    # else, assume rhel or centos or fedora or whatever that uses yum

    yum -y install \
        "RPMS/${arch}/${project}-${pkg_version}-${rel_version}.${arch}.rpm"
fi

if ldd $(which termsend) | grep "\/usr\/bofc"
then
    # sanity check to make sure test program uses system libraries
    # and not locally installed ones (which are used as build
    # dependencies for other programs

    echo "test prog uses libs from manually installed /usr/bofc \
        instead of system path!"
    exit 1
fi

termsend -v
/etc/init.d/termsend start
/etc/init.d/termsend restart
/etc/init.d/termsend stop

# cleanup
atexit "no-exit"

# run test prog again, but now fail if there is no error, since program
# should no longer be in the system.
retval=0
set +e
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

retval=1
if [ -n "${scp_server}" ]
then
    echo "copying data to ${scp_server}:${project}/${host_os}/${arch}"
    scp "RPMS/${arch}/${project}-${pkg_version}-${rel_version}.${arch}.rpm" \
        "RPMS/${arch}/${project}-debuginfo-${pkg_version}-${rel_version}.${arch}.rpm" \
        "${scp_server}:${project}/${host_os}/${arch}" || exit 1
fi

retval=0

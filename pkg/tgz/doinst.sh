#!/bin/sh

set -e

if ! getent group termsend >/dev/null; then
    groupadd --system termsend
fi

if ! getent passwd termsend >/dev/null; then
    useradd --system --home-dir /var/lib/termsend --shell /bin/false \
        --gid $(getent group termsend | cut -f3 -d:) termsend
fi

mkdir -p /var/lib/termsend
chown termsend:termsend /var/lib/termsend

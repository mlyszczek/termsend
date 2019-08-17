#!/bin/sh

set -e

if ! getent group kurload >/dev/null; then
    groupadd --system kurload
fi

if ! getent passwd kurload >/dev/null; then
    useradd --system --home-dir /var/lib/kurload --shell /bin/false \
        --gid $(getent group kurload | cut -f3 -d:) kurload
fi

mkdir -p /var/lib/kurload
chown kurload:kurload /var/lib/kurload

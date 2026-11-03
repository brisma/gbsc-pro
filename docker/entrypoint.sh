#!/bin/sh
# GBSC PRO container entry: run as the owner of the mounted repository (/work), so nothing it writes there belongs to
# root; dev.py commands go to tools/dev.py, anything else runs as given. Named volumes start owned by root (or by the
# image's dev user): they are handed to that user first (only their top directory: they are new and empty then).
set -e
case "$1" in
  doctor|check|build|flash) set -- python3 /work/tools/dev.py "$@" ;;
esac
uid=$(stat -c %u /work 2>/dev/null || echo 0)
gid=$(stat -c %g /work 2>/dev/null || echo 0)
if [ "$(id -u)" != 0 ] || [ "$uid" = 0 ]; then exec "$@"; fi
for d in /cache/pio-build /cache/target /opt/cargo/registry /opt/cargo/git \
         /work/gbs-control/public/node_modules /work/gbsc-pro-flasher/ui/node_modules; do
  mkdir -p "$d"
  [ "$(stat -c %u "$d")" = "$uid" ] || chown "$uid:$gid" "$d"
done
home=$(getent passwd "$uid" | cut -d: -f6)
if [ -z "$home" ] || ! setpriv --reuid "$uid" --regid "$gid" --clear-groups test -w "$home"; then
  home=/tmp/home-$uid; mkdir -p "$home"; chown "$uid:$gid" "$home"
fi
export HOME="$home"
exec setpriv --reuid "$uid" --regid "$gid" --clear-groups "$@"

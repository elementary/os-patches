#!/bin/sh
set -eu

export HOME="$(pwd)/debian/HOME"
# Put these back to their defaults if we are not running with a clean
# environment, so that they are based on the temporary $HOME above.
unset XDG_CACHE_HOME
unset XDG_CONFIG_DIRS
unset XDG_CONFIG_HOME
unset XDG_DATA_HOME
unset XDG_DATA_DIRS
# dconf assumes this directory exists and is writable
export XDG_RUNTIME_DIR="$(pwd)/debian/XDG_RUNTIME_DIR"

adverb=

if [ "$DEB_HOST_ARCH_BITS" = 64 ]; then
    # reprotest sometimes uses linux32 even for x86_64 builds, and
    # Flatpak's tests don't support this.
    adverb=linux64
fi

e=0
$adverb dh_auto_test || e=$?

find . -name 'test*.log' \
-not -name test-suite.log \
-print0 | xargs -0 tail -v -c1M

echo "Killing gpg-agent processes:"
pgrep --list-full --full "gpg-agent --homedir /var/tmp/test-flatpak-.*" >&2 || :
pgrep --list-full --full "gpg-agent --homedir /var/tmp/flatpak-test-.*" >&2 || :
pkill --full "gpg-agent --homedir /var/tmp/test-flatpak-.*" >&2 || :
pkill --full "gpg-agent --homedir /var/tmp/flatpak-test-.*" >&2 || :
exit "$e"

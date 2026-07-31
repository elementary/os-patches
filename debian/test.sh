#!/bin/sh
set -eu

# Some build/test infrastructure provides internet access via a proxy.
# libostree doesn't always support no_proxy (and in any case
# reproducible-builds.org doesn't set it), so tests will try to use the
# proxy for localhost, and fail to reach the test server.
unset ftp_proxy
unset http_proxy
unset https_proxy
unset no_proxy

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

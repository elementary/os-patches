#!/bin/sh
set -e

echo "Running PEP 8 test"
python3 tests/testmanual_pep8.py || [ "$IGNORE_PEP8" ]

dpkg-checkbuilddeps -d 'python-debian, python3-feedparser'

echo "updating Ubuntu mirror list from launchpad"
if [ -n "$https_proxy" ]; then
    echo "disabling https_proxy as Python's urllib doesn't support it; see #94130"
    unset https_proxy
fi
utils/get_ubuntu_mirrors_from_lp.py > data/templates/Ubuntu.mirrors

echo "updating Debian mirror list"
utils/get_debian_mirrors.py > data/templates/Debian.mirrors

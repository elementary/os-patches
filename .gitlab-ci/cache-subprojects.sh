#!/bin/bash

set -e

git clone https://gitlab.freedesktop.org/pwithnall/malcontent.git
meson subprojects download --sourcedir malcontent
rm malcontent/subprojects/*.wrap
mv malcontent/subprojects/ .
rm -rf malcontent

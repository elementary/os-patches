#!/usr/bin/python3
#
# usage: ./utils/expand_info.py build/data/templates/Debian.info \
#                               /usr/share/distro-info/debian.csv

import sys

import aptsources.distinfo

for line in aptsources.distinfo._expand_template(sys.argv[1], sys.argv[2]):
    print(line)

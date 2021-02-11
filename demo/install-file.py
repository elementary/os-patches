#!/usr/bin/env python3

# A simple script that copies a given file (first arg) to a given location
# (second arg).

import sys
import os
from shutil import copy

if len(sys.argv) < 3:
    print('Usage: ' + sys.argv[0] + ' SOURCE_FILE DESTINATION_DIR')

    sys.exit(-1)

try:
    dest_dir = os.environ['DESTDIR'] + '/' + sys.argv[2]
except KeyError:
    dest_dir = sys.argv[2]

try:
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
except OSError:
    print ('Error: Creating directory. ' +  dest_dir)

copy(sys.argv[1], dest_dir)

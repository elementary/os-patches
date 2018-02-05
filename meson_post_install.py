#!/usr/bin/env python3

import glob
import os
import re
import subprocess
import sys

name_pattern = re.compile('hicolor_(?:apps|status)_(?:\d+x\d+|scalable)_(.*)')
search_pattern = '/**/hicolor_*'

icon_dir = os.path.join(os.environ['MESON_INSTALL_PREFIX'], 'share', 'icons', 'hicolor')
[os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))
 for file in glob.glob(icon_dir + search_pattern, recursive=True)]

if not os.environ.get('DESTDIR') and sys.argv[1] == 'icon-update':
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icon_dir])

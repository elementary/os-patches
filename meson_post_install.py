#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys

if not os.environ.get('DESTDIR'):
  schemadir = os.path.join(sys.argv[1], 'glib-2.0', 'schemas')
  print('Compile gsettings schemas...')
  subprocess.call(['glib-compile-schemas', schemadir])

# FIXME: this is due to unable to copy a generated target file:
#        https://groups.google.com/forum/#!topic/mesonbuild/3iIoYPrN4P0
dst_dir = os.path.join(sys.argv[2], 'xdg', 'autostart')
src = os.path.join(sys.argv[1], 'applications', 'nm-applet.desktop')
if os.environ.get('DESTDIR'):
  dst_dir = os.environ.get('DESTDIR') + os.path.join(os.getcwd(), dst_dir)
  src = os.environ.get('DESTDIR') + os.path.join(os.getcwd(), src)
if not os.path.exists(dst_dir):
  os.makedirs(dst_dir)
dst = os.path.join(dst_dir, 'nm-applet.desktop')
shutil.copyfile(src, dst)

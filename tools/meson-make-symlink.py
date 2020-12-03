#!/usr/bin/env python3

import os
import sys

destdir = os.environ.get('DESTDIR')
source = sys.argv[1]
target = sys.argv[2]

def prepend_destdir(path):
    if destdir is not None:
        if os.path.isabs(path):
            return os.path.join(destdir, path[1:])
        else:
            return os.path.join(destdir, path)
    else:
        return path

def call_symlink(src, dst):
    try:
        os.remove(dst)
    except:
        pass
    os.symlink(src, dst)

os.makedirs(os.path.dirname(prepend_destdir(target)), exist_ok=True)
if os.path.dirname(source) == '.':
    destdir_target = prepend_destdir(target)
    call_symlink(source, destdir_target)
else:
    destdir_source = prepend_destdir(source)
    destdir_target = prepend_destdir(target)
    destdir_target_dir = os.path.dirname(destdir_target)
    relative_source = os.path.relpath(destdir_source, destdir_target_dir)
    call_symlink(relative_source, destdir_target)

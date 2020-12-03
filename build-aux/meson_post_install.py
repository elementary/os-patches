#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import subprocess

install_prefix = os.environ['MESON_INSTALL_PREFIX']

if not os.environ.get('DESTDIR'):
    icon_cache_dir = os.path.join(install_prefix, 'share', 'icons', 'hicolor')
    if os.path.exists(icon_cache_dir):
        print('Updating icon cache…')
        subprocess.call(['gtk-update-icon-cache', '-qtf', icon_cache_dir])

    desktop_database_dir = os.path.join(install_prefix, 'share', 'applications')
    if os.path.exists(desktop_database_dir):
        print('Updating desktop database…')
        subprocess.call(['update-desktop-database', '-q', desktop_database_dir])

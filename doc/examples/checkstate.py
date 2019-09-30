#!/usr/bin/python
#
#
# this example is not usefull to find out about updated, upgradable packages
# use the depcache.py example for it (because a pkgPolicy is not used here)
#

import apt_pkg
apt_pkg.init()

cache = apt_pkg.Cache()
packages = cache.packages

uninstalled, updated, upgradable = {}, {}, {}

for package in packages:
    versions = package.version_list
    if not versions:
        continue
    version = versions[0]
    for other_version in versions:
        if apt_pkg.version_compare(version.ver_str, other_version.ver_str) < 0:
            version = other_version
    if package.current_ver:
        current = package.current_ver
        if apt_pkg.version_compare(current.ver_str, version.ver_str) < 0:
            upgradable[package.name] = version
            break
        else:
            updated[package.name] = current
    else:
        uninstalled[package.name] = version


for l in (uninstalled, updated, upgradable):
    print l.items()[0]

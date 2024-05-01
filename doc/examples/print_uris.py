#!/usr/bin/python3
#
# a example that prints the URIs of all upgradable packages
#

import apt

for pkg in apt.Cache():
    if pkg.is_upgradable:
        print(pkg.candidate.uri)

#!/usr/bin/python3

import apt

cache = apt.Cache()

for pkg in cache:
    if pkg.installed and pkg.installed.enhances:
        s = "%s enhances:" % pkg.name
        for or_list in pkg.installed.enhances:
            for enhances in or_list.or_dependencies:
                s += " %s" % enhances.name
                if (enhances.name in cache and
                        not cache[enhances.name].is_installed):
                    s += "(*missing*) "
                s += ","
            print(s[:-1])

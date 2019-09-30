#!/usr/bin/env python
#
# a example that prints the URIs of all upgradable packages
#

import apt
import apt_pkg


cache = apt.Cache()
upgradable = filter(lambda p: p.is_upgradable, cache)


for pkg in upgradable:
    pkg._lookupRecord(True)
    path = apt_pkg.TagSection(pkg._records.record)["Filename"]
    cand = pkg._depcache.get_candidate_ver(pkg._pkg)
    for (packagefile, i) in cand.file_list:
        indexfile = cache._list.find_index(packagefile)
        if indexfile:
            uri = indexfile.archive_uri(path)
            print uri

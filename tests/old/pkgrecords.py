#!/usr/bin/env python2.4
#
# Test for the PkgSrcRecords code
# it segfaults for python-apt < 0.5.37
#

import apt_pkg
import sys


def main():
    apt_pkg.init()
    cache = apt_pkg.Cache()
    depcache = apt_pkg.DepCache(cache)
    depcache.init()
    i = 0
    print "Running PkgRecords test on all packages:"
    for pkg in cache.packages:
        i += 1
        records = apt_pkg.PackageRecords(cache)
        if len(pkg.version_list) == 0:
            #print "no available version, cruft"
            continue
        version = depcache.get_candidate_ver(pkg)
        if not version:
            continue
        file, index = version.file_list.pop(0)
        if records.lookup((file, index)):
            #print records.filename
            x = records.filename
            y = records.long_desc
            pass
        print "\r%i/%i=%.3f%%    " % (
            i, cache.package_count,
            (float(i) / float(cache.package_count) * 100)),


if __name__ == "__main__":
    main()
    sys.exit(0)

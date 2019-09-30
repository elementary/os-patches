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
    i = 0
    print "Running PkgSrcRecords test on all packages:"
    for x in cache.packages:
        i += 1
        src = apt_pkg.SourceRecords()
        if src.lookup(x.name):
            #print src.package
            pass
        print "\r%i/%i=%.3f%%    " % (
            i, cache.package_count,
            (float(i) / float(cache.package_count) * 100)),


if __name__ == "__main__":
    main()
    sys.exit(0)

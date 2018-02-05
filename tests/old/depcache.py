#!/usr/bin/env python2.4
#
# Test for the DepCache code
#

import apt_pkg
import sys


def main():
    apt_pkg.init()
    cache = apt_pkg.Cache()
    depcache = apt_pkg.DepCache(cache)
    depcache.init()
    i = 0
    all = cache.package_count
    print "Running DepCache test on all packages"
    print "(trying to install each and then mark it keep again):"
    # first, get all pkgs
    for pkg in cache.packages:
        i += 1
        x = pkg.name
        # then get each version
        ver = depcache.get_candidate_ver(pkg)
        if ver is not None:
            depcache.mark_install(pkg)
            if depcache.inst_count == 0:
                if depcache.is_upgradable(pkg):
                    print "Error marking %s for install" % x
            for p in cache.packages:
                if depcache.marked_install(p):
                    depcache.mark_keep(p)
            if depcache.inst_count != 0:
                print "Error undoing the selection for %s (inst_count: %s)" % (
                    x, depcache.inst_count)
        print "\r%i/%i=%.3f%%    " % (i, all, (float(i) / float(all) * 100)),

    print
    print "Trying upgrade:"
    depcache.upgrade()
    print "To install: %s " % depcache.inst_count
    print "To remove: %s " % depcache.del_count
    print "Kept back: %s " % depcache.keep_count

    print "Trying DistUpgrade:"
    depcache.upgrade(True)
    print "To install: %s " % depcache.inst_count
    print "To remove: %s " % depcache.del_count
    print "Kept back: %s " % depcache.keep_count


if __name__ == "__main__":
    main()
    sys.exit(0)

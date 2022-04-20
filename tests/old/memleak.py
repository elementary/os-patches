#!/usr/bin/python3

import apt
import apt_pkg
import time
import gc
import sys


cache = apt.Cache()

# memleak
for i in range(100):
    cache.open(None)
    print(cache["apt"].name)
    time.sleep(1)
    gc.collect()
    f = open("%s" % i, "w")
    for obj in gc.get_objects():
        f.write("%s\n" % str(obj))
    f.close()

# memleak
#for i in range(100):
#       cache = apt.Cache()
#       time.sleep(1)
#       cache = None
#       gc.collect()

# no memleak, but more or less the apt.Cache.open() code
for i in range(100):
    cache = apt_pkg.Cache()
    depcache = apt_pkg.DepCache(cache)
    records = apt_pkg.PackageRecords(cache)
    list = apt_pkg.SourceList()
    list.read_main_list()
    dict = {}
    for pkg in cache.packages:
        if len(pkg.version_list) > 0:
            dict[pkg.name] = apt.package(cache, depcache,
                                         records, list, None, pkg)

    print(cache["apt"])
    time.sleep(1)

    gc.collect()

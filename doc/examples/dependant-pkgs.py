#!/usr/bin/env python

import apt
import sys

pkgs = set()
cache = apt.Cache()
for pkg in cache:
    candver = cache._depcache.get_candidate_ver(pkg._pkg)
    if candver is None:
        continue
    dependslist = candver.depends_list
    for dep in dependslist.keys():
        # get the list of each dependency object
        for depVerList in dependslist[dep]:
            for z in depVerList:
                # get all TargetVersions of
                # the dependency object
                for tpkg in z.all_targets():
                    if sys.argv[1] == tpkg.parent_pkg.name:
                        pkgs.add(pkg.name)

main = set()
universe = set()
for pkg in pkgs:
    if "universe" in cache[pkg].section:
        universe.add(cache[pkg].source_name)
    else:
        main.add(cache[pkg].source_name)

print "main:"
print "\n".join(main)
print

print "universe:"
print "\n".join(universe)

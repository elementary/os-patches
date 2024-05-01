#!/usr/bin/python3

import sys

import apt

pkgs = set()
cache = apt.Cache()
for pkg in cache:
    candver = cache._depcache.get_candidate_ver(pkg._pkg)
    if candver is None:
        continue
    dependslist = candver.depends_list
    for dep in list(dependslist.keys()):
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
    cand = cache[pkg].candidate
    if "universe" in cand.section:
        universe.add(cand.source_name)
    else:
        main.add(cand.source_name)

print("main:")
print("\n".join(sorted(main)))
print()

print("universe:")
print("\n".join(sorted(universe)))

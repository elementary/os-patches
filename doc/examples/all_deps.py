#!/usr/bin/env python
import sys

import apt


def dependencies(cache, pkg, deps, key="Depends"):
    #print "pkg: %s (%s)" % (pkg.name, deps)
    candver = cache._depcache.get_candidate_ver(pkg._pkg)
    if candver is None:
        return deps
    dependslist = candver.depends_list
    if key in dependslist:
        for depVerList in dependslist[key]:
            for dep in depVerList:
                if dep.target_pkg.name in cache:
                    if (pkg.name != dep.target_pkg.name and
                            dep.target_pkg.name not in deps):
                        deps.add(dep.target_pkg.name)
                        dependencies(
                            cache, cache[dep.target_pkg.name], deps, key)
    return deps


pkgname = sys.argv[1]
c = apt.Cache()
pkg = c[pkgname]

deps = set()

deps = dependencies(c, pkg, deps, "Depends")
print " ".join(deps)

preDeps = set()
preDeps = dependencies(c, pkg, preDeps, "PreDepends")
print " ".join(preDeps)

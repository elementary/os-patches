#!/usr/bin/python
# this is a example how to access the build dependencies of a package

import apt_pkg
import sys


def get_source_pkg(pkg, records, depcache):
    """ get the source package name of a given package """
    version = depcache.get_candidate_ver(pkg)
    if not version:
        return None
    file, index = version.file_list.pop(0)
    records.lookup((file, index))
    if records.source_pkg != "":
        srcpkg = records.source_pkg
    else:
        srcpkg = pkg.name
    return srcpkg


# main
apt_pkg.init()
cache = apt_pkg.Cache()
depcache = apt_pkg.DepCache(cache)
depcache.init()
records = apt_pkg.PackageRecords(cache)
srcrecords = apt_pkg.SourceRecords()

# base package that we use for build-depends calculation
if len(sys.argv) < 2:
    print "need a package name as argument"
    sys.exit(1)
try:
    pkg = base = cache[sys.argv[1]]
except KeyError:
    print "No package %s found" % sys.argv[1]
    sys.exit(1)
all_build_depends = set()

# get the build depdends for the package itself
srcpkg_name = get_source_pkg(base, records, depcache)
print "srcpkg_name: %s " % srcpkg_name
if not srcpkg_name:
    print "Can't find source package for '%s'" % pkg.mame
srcrec = srcrecords.lookup(srcpkg_name)
if srcrec:
    print "Files:"
    print srcrecords.files
    bd = srcrecords.build_depends
    print "build-depends of the package: %s " % bd
    for b in bd:
        all_build_depends.add(b[0])

# calculate the build depends for all dependencies
depends = depcache.get_candidate_ver(base).depends_list
for dep in depends["Depends"]:  # FIXME: do we need to consider PreDepends?
    pkg = dep[0].target_pkg
    srcpkg_name = get_source_pkg(pkg, records, depcache)
    if not srcpkg_name:
        print "Can't find source package for '%s'" % pkg.name
        continue
    srcrec = srcrecords.lookup(srcpkg_name)
    if srcrec:
        #print srcrecords.package
        #print srcrecords.binaries
        bd = srcrecords.build_depends
        #print "%s: %s " % (srcpkg_name, bd)
        for b in bd:
            all_build_depends.add(b[0])


print "\n".join(all_build_depends)

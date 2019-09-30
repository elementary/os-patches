#!/usr/bin/python

import apt_pkg

apt_pkg.init()

sources = apt_pkg.SourceList()
sources.read_main_list()

cache = apt_pkg.Cache()
depcache = apt_pkg.DepCache(cache)
pkg = cache["libimlib2"]
cand = depcache.get_candidate_ver(pkg)
for (f, i) in cand.file_list:
    index = sources.find_index(f)
    print index
    if index:
        print index.size
        print index.is_trusted
        print index.exists
        print index.Haspackages
        print index.archive_uri("some/path")

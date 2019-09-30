#!/usr/bin/python

import apt_pkg

apt_pkg.init()

apt_pkg.config.set("APT::Acquire::Translation", "de")

cache = apt_pkg.Cache()
depcache = apt_pkg.DepCache(cache)

pkg = cache["gcc"]
cand = depcache.get_candidate_ver(pkg)
print cand

desc = cand.TranslatedDescription
print desc
print desc.file_list
(f, index) = desc.file_list.pop(0)

records = apt_pkg.PackageRecords(cache)
records.lookup((f, index))
desc = records.long_desc
print len(desc)
print desc

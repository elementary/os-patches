#!/usr/bin/python

import apt_pkg

apt_pkg.init()

#cache = apt_pkg.Cache()
#sources = apt_pkg.SourceRecords(cache)

sources = apt_pkg.SourceRecords()
sources.restart()
while sources.lookup('hello'):
    print sources.package, sources.version, sources.maintainer, \
        sources.section, repr(sources.binaries)
    print sources.files
    print sources.index.archive_uri(sources.files[0][2])

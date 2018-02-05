#!/usr/bin/python

import apt_pkg

apt_pkg.init()

sources = apt_pkg.SourceList()
sources.read_main_list()


for metaindex in sources.list:
    print metaindex
    print "uri: ", metaindex.uri
    print "dist: ", metaindex.dist
    print "index_files: ", "\n".join([str(i) for i in metaindex.index_files])
    print

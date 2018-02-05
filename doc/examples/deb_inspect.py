#!/usr/bin/env python
# some example for apt_inst

import apt_pkg
import apt_inst
import sys
import os.path


def Callback(member, data):
    """ callback for debExtract """
    print "'%s','%s',%u,%u,%u,%u,%u,%u,%u" \
          % (member.name, member.linkname, member.mode, member.uid,
             member.gid, member.size, member.mtime, member.major, member.minor)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "need filename argumnet"
        sys.exit(1)
    file = sys.argv[1]

    print "Working on: %s" % file
    print "Displaying data.tar.gz:"
    apt_inst.DebFile(open(file)).data.go(Callback)

    print "Now extracting the control file:"
    control = apt_inst.DebFile(open(file)).control.extractdata("control")
    sections = apt_pkg.TagSection(control)

    print "Maintainer is: "
    print sections["Maintainer"]

    print
    print "DependsOn: "
    depends = sections["Depends"]
    print apt_pkg.parse_depends(depends)

    print "extracting archive"
    dir = "/tmp/deb"
    os.mkdir(dir)
    apt_inst.DebFile(open(file)).data.extractall(dir)

    def visit(arg, dirname, names):
        print "%s/" % dirname
        for file in names:
            print "\t%s" % file

    os.path.walk(dir, visit, None)

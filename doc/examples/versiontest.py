#!/usr/bin/python

# This is a simple clone of tests/versiontest.cc
import apt_pkg
import sys
import re

apt_pkg.init_config()
apt_pkg.init_system()

TestFile = apt_pkg.parse_commandline(apt_pkg.config, [], sys.argv)
if len(TestFile) != 1:
    print "Must have exactly 1 file name"
    sys.exit(0)

# Go over the file..
list = open(TestFile[0], "r")
CurLine = 0
while(1):
    Line = list.readline()
    CurLine = CurLine + 1
    if Line == "":
        break
    Line = Line.strip()
    if len(Line) == 0 or Line[0] == '#':
        continue

    Split = re.split("[ \n]", Line)

    # Check forward
    if apt_pkg.version_compare(Split[0], Split[1]) != int(Split[2]):
        print "Comparision failed on line %u. '%s' ? '%s' %i != %i" % (CurLine,
            Split[0], Split[1], apt_pkg.version_compare(Split[0], Split[1]),
            int(Split[2]))
    # Check reverse
    if apt_pkg.version_compare(Split[1], Split[0]) != -1 * int(Split[2]):
        print "Comparision failed on line %u. '%s' ? '%s' %i != %i" % (CurLine,
            Split[1], Split[0], apt_pkg.version_compare(Split[1], Split[0]),
            -1 * int(Split[2]))

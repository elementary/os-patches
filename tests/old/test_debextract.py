#!/usr/bin/python
import sys

import apt_inst


def Callback(What, name, Link, Mode, UID, GID, size, MTime, Major, Minor):
    print "%s '%s','%s',%u,%u,%u,%u,%u,%u,%u" % (
        What, name, Link, Mode, UID, GID, size, MTime, Major, Minor)


def main():
    member = "data.tar.lzma"
    if len(sys.argv) > 2:
        member = sys.argv[2]
    apt_inst.debExtract(open(sys.argv[1]), Callback, member)

main()

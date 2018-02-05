#!/usr/bin/python
"""Emulate dpkg --contents"""

from __future__ import print_function

import grp
import pwd
import stat
import sys
import time

import apt_inst


def format_mode(member):
    """Return the symbolic mode"""
    mode = member.mode
    if member.isdir():
        s_mode = "d"
    elif member.islnk():
        s_mode = "h"
    else:
        s_mode = "-"
    s_mode += ((mode & stat.S_IRUSR) and "r" or "-")
    s_mode += ((mode & stat.S_IWUSR) and "w" or "-")
    s_mode += ((mode & stat.S_IXUSR) and
               (mode & stat.S_ISUID and "s" or "x") or
               (mode & stat.S_ISUID and "S" or "-"))
    s_mode += ((mode & stat.S_IRGRP) and "r" or "-")
    s_mode += ((mode & stat.S_IWGRP) and "w" or "-")
    s_mode += ((mode & stat.S_IXGRP) and
               (mode & stat.S_ISGID and "s" or "x") or
               (mode & stat.S_ISGID and "S" or "-"))
    s_mode += ((mode & stat.S_IROTH) and "r" or "-")
    s_mode += ((mode & stat.S_IWOTH) and "w" or "-")
    s_mode += ((mode & stat.S_IXOTH) and "x" or "-")
    return s_mode


def callback(member, data):
    """callback for deb_extract"""
    s_mode = format_mode(member)
    s_owner = "%s/%s" % (pwd.getpwuid(member.uid)[0],
                         grp.getgrgid(member.gid)[0])
    s_size = "%9d" % member.size
    s_time = time.strftime("%Y-%m-%d %H:%M", time.localtime(member.mtime))
    s_name = (member.name if member.name.startswith(".")
              else ("./" + member.name))
    if member.islnk():
        s_name += " link to %s" % member.linkname
    print(s_mode, s_owner, s_size, s_time, s_name)


def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("need filename argumnet", file=sys.stderr)
        sys.exit(1)

    fobj = open(sys.argv[1])
    try:
        apt_inst.DebFile(fobj).data.go(callback)
    finally:
        fobj.close()

if __name__ == "__main__":
    main()

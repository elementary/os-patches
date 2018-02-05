#!/usr/bin/python
"""Emulate dpkg --extract package.deb outdir"""

from __future__ import print_function

import os
import sys

import apt_inst


def main():
    """Main function."""
    if len(sys.argv) < 3:
        print("Usage: %s package.deb outdir\n" % (__file__), file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(sys.argv[2]):
        print("The directory %s does not exist\n" % (sys.argv[2]),
              file=sys.stderr)
        sys.exit(1)

    fobj = open(sys.argv[1])
    try:
        apt_inst.DebFile(fobj).data.extractall(sys.argv[2])
    finally:
        fobj.close()

if __name__ == "__main__":
    main()

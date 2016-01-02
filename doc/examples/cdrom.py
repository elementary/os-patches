#!/usr/bin/python
# example how to deal with the depcache

import apt_pkg
import sys

from progress import TextCdromProgress


# init
apt_pkg.init()

cdrom = apt_pkg.Cdrom()
print cdrom

progress = TextCdromProgress()

(res, ident) = cdrom.ident(progress)
print "ident result is: %s (%s) " % (res, ident)

apt_pkg.config["APT::CDROM::Rename"] = "True"
cdrom.add(progress)

print "Exiting"
sys.exit(0)

#!/usr/bin/python3
# example how to deal with the depcache

import sys

import apt_pkg
from progress import TextCdromProgress

# init
apt_pkg.init()

cdrom = apt_pkg.Cdrom()
print(cdrom)

progress = TextCdromProgress()

(res, ident) = cdrom.ident(progress)
print(f"ident result is: {res} ({ident}) ")

apt_pkg.config["APT::CDROM::Rename"] = "True"
cdrom.add(progress)

print("Exiting")
sys.exit(0)

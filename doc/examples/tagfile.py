#!/usr/bin/env python
import apt_pkg

Parse = apt_pkg.TagFile(open("/var/lib/dpkg/status", "r"))

while Parse.step() == 1:
    print Parse.section.get("Package")
    print apt_pkg.parse_depends(Parse.section.get("Depends", ""))

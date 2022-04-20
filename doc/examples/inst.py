#!/usr/bin/python3
# example how to deal with the depcache

import apt
import sys

from apt.progress import InstallProgress


class TextInstallProgress(InstallProgress):

    def __init__(self):
        apt.progress.InstallProgress.__init__(self)
        self.last = 0.0

    def updateInterface(self):
        InstallProgress.updateInterface(self)
        if self.last >= self.percent:
            return
        sys.stdout.write("\r[%s] %s\n" % (self.percent, self.status))
        sys.stdout.flush()
        self.last = self.percent

    def conffile(self, current, new):
        print("conffile prompt: %s %s" % (current, new))

    def error(self, errorstr):
        print("got dpkg error: '%s'" % errorstr)


cache = apt.Cache(apt.progress.OpTextProgress())

fprogress = apt.progress.TextFetchProgress()
iprogress = TextInstallProgress()

pkg = cache["3dchess"]

# install or remove, the importend thing is to keep us busy :)
if pkg.is_installed:
    print("Going to delete %s" % pkg.name)
    pkg.mark_delete()
else:
    print("Going to install %s" % pkg.name)
    pkg.mark_install()
res = cache.commit(fprogress, iprogress)
print(res)

sys.exit(0)

#!/usr/bin/python
import apt
import apt.progress.text
import apt_pkg
import os


def get_file(fetcher, uri, destfile):
    # get the file
    af = apt_pkg.AcquireFile(fetcher, uri=uri, descr="sample descr",
                               destfile=destfile)
    print "desc_uri: %s -> %s" % (af.desc_uri, af.destfile)
    res = fetcher.run()
    if res != fetcher.RESULT_CONTINUE:
        return False
    return True

apt_pkg.init()

#apt_pkg.config.set("Debug::pkgDPkgPM","1");
#apt_pkg.config.set("Debug::pkgPackageManager","1");
#apt_pkg.config.set("Debug::pkgDPkgProgressReporting","1");

cache = apt_pkg.Cache()
depcache = apt_pkg.DepCache(cache)

recs = apt_pkg.PackageRecords(cache)
list = apt_pkg.SourceList()
list.read_main_list()

# show the amount fetch needed for a dist-upgrade
depcache.upgrade(True)
progress = apt.progress.text.AcquireProgress()
fetcher = apt_pkg.Acquire(progress)
pm = apt_pkg.PackageManager(depcache)
pm.get_archives(fetcher, list, recs)
print "%s (%s)" % (
    apt_pkg.size_to_str(fetcher.fetch_needed), fetcher.fetch_needed)
actiongroup = apt_pkg.ActionGroup(depcache)
for pkg in cache.packages:
    depcache.mark_keep(pkg)

try:
    os.mkdir("/tmp/pyapt-test")
    os.mkdir("/tmp/pyapt-test/partial")
except OSError:
    pass
apt_pkg.config.set("Dir::Cache::archives", "/tmp/pyapt-test")

pkg = cache["2vcard"]
depcache.mark_install(pkg)

progress = apt.progress.text.AcquireProgress()
fetcher = apt_pkg.Acquire(progress)
#fetcher = apt_pkg.Acquire()
pm = apt_pkg.PackageManager(depcache)

print pm
print fetcher

get_file(fetcher, "ftp://ftp.debian.org/debian/dists/README", "/tmp/lala")

pm.get_archives(fetcher, list, recs)

for item in fetcher.items:
    print item
    if item.status == item.STAT_ERROR:
        print "Some error ocured: '%s'" % item.error_text
    if not item.complete:
        print "No error, still nothing downloaded (%s)" % item.error_text
    print


res = fetcher.run()
print "fetcher.Run() returned: %s" % res

print "now runing pm.DoInstall()"
res = pm.do_install(1)
print "pm.DoInstall() returned: %s" % res

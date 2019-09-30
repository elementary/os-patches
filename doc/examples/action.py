#!/usr/bin/python
# example how to deal with the depcache

import apt_pkg
import sys
from apt.progress.text import OpProgress
from progress import TextFetchProgress

# init
apt_pkg.init()

progress = OpProgress()
cache = apt_pkg.Cache(progress)
print "Available packages: %s " % cache.package_count

print "Fetching"
progress = TextFetchProgress()
cache.update(progress)

print "Exiting"
sys.exit(0)

iter = cache["base-config"]
print "example package iter: %s" % iter

# get depcache
print "\n\n depcache"
depcache = apt_pkg.DepCache(cache, progress)
depcache.read_pin_file()
print "got a depcache: %s " % depcache
print "Marked for install: %s " % depcache.inst_count

print "\n\n Reinit"
depcache.init(progress)

#sys.exit()

# get a canidate version
ver = depcache.get_candidate_ver(iter)
print "Candidate version: %s " % ver

print "\n\nQuerry interface"
print "%s.is_upgradable(): %s" % (iter.name, depcache.is_upgradable(iter))

print "\nMarking interface"
print "Marking '%s' for install" % iter.name
depcache.mark_install(iter)
print "Install count: %s " % depcache.inst_count
print "%s.marked_install(): %s" % (iter.name, depcache.marked_install(iter))
print "%s.marked_upgrade(): %s" % (iter.name, depcache.marked_upgrade(iter))
print "%s.marked_delete(): %s" % (iter.name, depcache.marked_delete(iter))

print "Marking %s for delete" % iter.name
depcache.mark_delete(iter)
print "del_count: %s " % depcache.del_count
print "%s.marked_delete(): %s" % (iter.name, depcache.marked_delete(iter))

iter = cache["3dchess"]
print "\nMarking '%s' for install" % iter.name
depcache.mark_install(iter)
print "Install count: %s " % depcache.inst_count
print "%s.marked_install(): %s" % (iter.name, depcache.marked_install(iter))
print "%s.marked_upgrade(): %s" % (iter.name, depcache.marked_upgrade(iter))
print "%s.marked_delete(): %s" % (iter.name, depcache.marked_delete(iter))

print "Marking %s for keep" % iter.name
depcache.mark_keep(iter)
print "Install: %s " % depcache.inst_count

iter = cache["3dwm-server"]
print "\nMarking '%s' for install" % iter.name
depcache.mark_install(iter)
print "Install: %s " % depcache.inst_count
print "Broken count: %s" % depcache.broken_count
print "fix_broken() "
depcache.fix_broken()
print "Broken count: %s" % depcache.broken_count

print "\nPerforming upgrade"
depcache.upgrade()
print "Keep: %s " % depcache.keep_count
print "Install: %s " % depcache.inst_count
print "Delete: %s " % depcache.del_count
print "usr_size: %s " % apt_pkg.size_to_str(depcache.usr_size)
print "deb_size: %s " % apt_pkg.size_to_str(depcache.deb_size)

for pkg in cache.packages:
    if pkg.current_ver is not None and not depcache.marked_install(pkg) \
        and depcache.is_upgradable(pkg):
        print "upgrade didn't upgrade (kept): %s" % pkg.name

print "\nPerforming Distupgrade"
depcache.upgrade(True)
print "Keep: %s " % depcache.keep_count
print "Install: %s " % depcache.inst_count
print "Delete: %s " % depcache.del_count
print "usr_size: %s " % apt_pkg.size_to_str(depcache.usr_size)
print "deb_size: %s " % apt_pkg.size_to_str(depcache.deb_size)

# overview about what would happen
for pkg in cache.packages:
    if depcache.marked_install(pkg):
        if pkg.current_ver is not None:
            print "Marked upgrade: %s " % pkg.name
        else:
            print "Marked install: %s" % pkg.name
    elif depcache.marked_delete(pkg):
        print "Marked delete: %s" % pkg.name
    elif depcache.marked_keep(pkg):
        print "Marked keep: %s" % pkg.name

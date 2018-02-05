#!/usr/bin/python-dbg

from pprint import pprint
import apt
import sys
import gc

# get initial cache
print sys.gettotalrefcount()
progress = apt.progress.OpTextProgress()
c = apt.Cache(progress)
print "refcount after first cache instance: ", sys.gettotalrefcount()

# test open()
c.open(progress)
print "refcount after cache open: ", sys.gettotalrefcount()
#pprint(sys.getobjects(10))

c.open(apt.progress.OpProgress())
print "refcount after seconf cache open: ", sys.gettotalrefcount()
#pprint(sys.getobjects(10))

# FIXME: find a way to get a efficient diff
#before = gc.get_objects()
#c.open(apt.progress.OpProgress())
#after = gc.get_objects()


# test update()
print "refcount before cache.update(): ", sys.gettotalrefcount()
c.update()
gc.collect()
print "refcount after cache.update(): ", sys.gettotalrefcount()
c.update()
gc.collect()
print "refcount after second cache.update(): ", sys.gettotalrefcount()
#pprint(sys.getobjects(20))


# test install()
c.open(apt.progress.OpProgress())
gc.collect()
print "refcount before cache['hello'].mark_install(): ", sys.gettotalrefcount()
c["hello"].mark_install()
c.commit(apt.progress.FetchProgress(), apt.progress.InstallProgress())
gc.collect()
print "refcount after: ", sys.gettotalrefcount()
c.open(apt.progress.OpProgress())
c["hello"].mark_delete()
c.commit(apt.progress.FetchProgress(), apt.progress.InstallProgress())
gc.collect()
print "refcount after: ", sys.gettotalrefcount()
pprint(sys.getobjects(10))

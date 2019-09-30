import warnings
warnings.filterwarnings("ignore", "apt API not stable yet", FutureWarning)
import apt


if __name__ == "__main__":
    progress = apt.progress.OpTextProgress()
    cache = apt.Cache(progress)
    print cache
    for pkg in cache:
        if pkg.is_upgradable:
            pkg.mark_install()
    for pkg in cache.get_changes():
        #print pkg.name()
        pass
    print "Broken: %s " % cache._depcache.broken_count
    print "inst_count: %s " % cache._depcache.inst_count

    # get a new cache
    cache = apt.Cache(progress)
    for name in cache.keys():
        import random
        if random.randint(0, 1) == 1:
            cache[name].mark_delete()
    print "Broken: %s " % cache._depcache.broken_count
    print "del_count: %s " % cache._depcache.del_count

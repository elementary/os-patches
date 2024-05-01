#!/usr/bin/python3
#
# Test for the pkgCache code
#

import sys

import apt_pkg


def main():
    apt_pkg.init()
    cache = apt_pkg.Cache()
    depcache = apt_pkg.DepCache(cache)
    depcache.init()
    i = 0
    all = cache.package_count
    print("Running Cache test on all packages:")
    # first, get all pkgs
    for pkg in cache.packages:
        i += 1
        pkg.name
        # then get each version
        for ver in pkg.version_list:
            # get some version information
            ver.file_list
            ver.ver_str
            ver.arch
            ver.depends_listStr
            dl = ver.depends_list
            # get all dependencies (a dict of string->list,
            # e.g. "depends:" -> [ver1,ver2,..]
            for dep in dl.keys():
                # get the list of each dependency object
                for depVerList in dl[dep]:
                    for z in depVerList:
                        # get all TargetVersions of
                        # the dependency object
                        for j in z.all_targets():
                            j.file_list
                            ver.ver_str
                            ver.arch
                            ver.depends_listStr
                            j = ver.depends_list

        print("\r%i/%i=%.3f%%    " % (i, all, (float(i) / float(all) * 100)))


if __name__ == "__main__":
    main()
    sys.exit(0)

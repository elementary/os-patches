#!/usr/bin/python

import apt_pkg
apt_pkg.init()

cache = apt_pkg.Cache()


class Wanted:

    def __init__(self, name):
        self.name = name
        self.recommended = []
        self.suggested = []


wanted = {}

for package in cache.packages:
    current = package.current_ver
    if not current:
        continue
    depends = current.depends_list
    for (key, attr) in (('Suggests', 'suggested'),
                        ('Recommends', 'recommended')):
        list = depends.get(key, [])
        for dependency in list:
            name = dependency[0].target_pkg.name
            dep = cache[name]
            if dep.current_ver:
                continue
            getattr(wanted.setdefault(name, Wanted(name)),
                    attr).append(package.name)

ks = wanted.keys()
ks.sort()

for want in ks:
    print want, wanted[want].recommended, wanted[want].suggested

#!/usr/bin/python
#  get_debian_mirrors.py - Parse Mirrors.masterlist and create a mirror list.
#
#  Copyright (c) 2010-2011 Julian Andres Klode <jak@debian.org>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA
from __future__ import print_function
import collections
import sys
import urllib2
from debian_bundle import deb822

mirrors = collections.defaultdict(set)
masterlist = urllib2.urlopen("https://mirror-master.debian.org/"
                             "status/Mirrors.masterlist")

for mirror in deb822.Deb822.iter_paragraphs(masterlist):
    if "Country" not in mirror:
        continue
    country = mirror["Country"].split(None, 1)[0]
    site = mirror["Site"]
    for proto in 'http', 'ftp':
        if "Archive-%s" % proto in mirror:
            mirrors[country].add("%s://%s%s" % (proto, site,
                                                mirror["Archive-%s" % proto]))

if len(mirrors) == 0:
    sys.stderr.write("E: Could not read the mirror list due to "
                     "some unknown issue\n")
    sys.exit(1)
for country in sorted(mirrors):
    print("#LOC:%s" % country)
    print("\n".join(sorted(mirrors[country])))

#!/usr/bin/env python3
#
#  get_ubuntu_lp_mirrors.py
#
#  Download the latest list with available Ubuntu mirrors from Launchpad.net
#  and extract the hosts from the raw page
#
#  Copyright (c) 2006 Free Software Foundation Europe
#
#  Author: Sebastian Heinlein <glatzor@ubuntu.com>
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

import sys
import feedparser

d = feedparser.parse("https://launchpad.net/ubuntu/+archivemirrors-rss")
#d = feedparser.parse(open("+archivemirrors-rss"))

countries = {}

for entry in d.entries:
    countrycode = entry.mirror_countrycode
    if countrycode not in countries:
        countries[countrycode] = set()
    for link in entry.links:
        countries[countrycode].add(link.href)


keys = sorted(countries)

if len(keys) == 0:
    sys.stderr.write("E: Could not read the mirror list due to some issue"
                     " -- status code: %s\n" % d.status)
    sys.exit(1)

print("mirror://mirrors.ubuntu.com/mirrors.txt")
for country in keys:
    print("#LOC:%s" % country)
    print("\n".join(sorted(countries[country])))

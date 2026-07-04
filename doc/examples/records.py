#!/usr/bin/python3

import apt

cache = apt.Cache()

for pkg in cache:
    if not pkg.candidate.record:
        continue
    if "Task" in pkg.candidate.record:
        print(
            "Pkg {} is part of '{}'".format(
                pkg.name, pkg.candidate.record["Task"].split()
            )
        )
        # print pkg.candidateRecord

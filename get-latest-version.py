#!/usr/bin/env python3

import os
import sys
import apt_pkg
from launchpadlib.launchpad import Launchpad
from github import Github

# Process the command line arguments
if len(sys.argv) < 2:
    raise ValueError("Please provide a package name")

if len(sys.argv) < 3:
    series_name = "bionic"
else:
    series_name = sys.argv[2]

if len(sys.argv) < 4:
    upstream_series_name = series_name
else:
    upstream_series_name = sys.argv[3]

component_name = sys.argv[1]

# Initialize APT
apt_pkg.init_system()

# Initialize Launchpad variables
launchpad = Launchpad.login_anonymously(
    'elementary daily test',
    'production',
    "~/.launchpadlib/cache/",
    version='devel'
)

ubuntu = launchpad.distributions["ubuntu"]
ubuntu_archive = ubuntu.main_archive
patches_archive = launchpad.people['elementary-os'].getPPAByName(distribution=ubuntu,name='os-patches')
series = ubuntu.getSeries(name_or_version=series_name)
upstream_series = ubuntu.getSeries(name_or_version=upstream_series_name)

# Initialize GitHub variables
github_token = os.environ['GITHUB_TOKEN']
github_repo = os.environ['GITHUB_REPOSITORY']
github = Github(github_token)

# Get the current version of a package in elementary os patches PPA
patched_sources = patches_archive.getPublishedSources(exact_match=True,
    source_name=component_name,
    status="Published",
    distro_series=series)
if len(patched_sources) == 0:
    raise ValueError("Package %s not found in elementary os-patches!" % (component_name))

patched_version = patched_sources[0].source_package_version

# Search for a new version in the Ubuntu repositories
pockets = ["Release", "Security", "Updates"]
for pocket in pockets:
    found_sources = ubuntu_archive.getPublishedSources(exact_match=True,
        source_name=component_name,
        status="Published",
        pocket=pocket,
        distro_series=upstream_series)
    if len(found_sources) > 0:
        pocket_version = found_sources[0].source_package_version
        if apt_pkg.version_compare(pocket_version, patched_version) > 0:
            repo = github.get_repo(github_repo)
            issue = repo.create_issue("New version of %s available" % (component_name), "The package %s can be upgraded to version %s" % (component_name, pocket_version))
            print("The patched package `%s` has a new version `%s` (was version `%s`) - Created issue %d" % (component_name, pocket_version, patched_version, issue.number))

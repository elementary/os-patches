# distro-info-data

The `distro-info` package provides centralized lists of code-names and
release history for the supported distributions (Currently: Debian and
Ubuntu).

The `distro-info` data (in the `distro-info-data` package) can be
updated once, and all the packages using it will have the latest
data. This avoids having to hard-code current development release names
(and other such volatile data) into packages.

## Outdated Data Errors

If you get an error that the package data is out of date, look for a newer
distro-info-data package in your distribution's updates.

On Debian, this is:
deb http://ftp.debian.org/debian stable-updates main

On Ubuntu, it is:
deb http://archive.ubuntu.com/ubuntu $RELEASE-updates main
where $RELEASE is the name of your release.

If there isn't an update available yet, you should be able to install the
latest version from Debian/unstable.

## Online data

Please don't scrape the git interface directly.

This data is available publicly at:

* https://debian.pages.debian.net/distro-info-data/debian.csv
* https://debian.pages.debian.net/distro-info-data/ubuntu.csv

## Data format

The data is in CSV format. They are parsed by code specific to the
distribution, so columns use and meaning vary.
Each row is a release in the distribution's history.

* `version`: Numeric (decimal) release version. Suffixed `LTS` for
  Ubuntu LTS releases.
* `codename`: Full human-readable name of the release.
* `series`: The machine-readable series name (suite).
* `created`: Date that the release started development. Normally the
  release date for the previous release.
* `release`: Official (stable) release date. Not defined when unknown
   and for suites that will never release (e.g. Debian unstable &
   experimental).
* `eol`: The primary End of Life date for the release. Excluding Debian
   LTS and Ubuntu ESM.
* `eol-server`: End of Life for use on servers. (Specific to early
   Ubuntu LTSs).

# OS Patches metarepository

This repository contains all the sources for the elementary specific patches

The packages are included into the [OS Patches Repository](https://launchpad.net/~elementary-os/+archive/ubuntu/os-patches).

## How is this repository working

The `master` branch is composed of GitHub Workflows that are running daily to
check for any new version in the Ubuntu repositories of the patched components.
It uses the `get-latest-version.py` python script included.

It depends on `python3-launchpadlib`, `python3-apt` and `python3-github`.

## Many branches

The repository is made of several distinc branches:
 * `import-list-$UBUNTU_NAME` is the branch listing the different packages
 that are getting patched.
 * `$PACKAGE-$UBUNTU_NAME` is the last source that got used to create the
 patched version
 * `$PACKAGE-$UBUNTU_NAME-patched` is `$PACKAGE-$UBUNTU_NAME` with the patch
 applied on it


> Note that when possible, we try to discourage the use of OS patches and work
directly with upstream to include them.

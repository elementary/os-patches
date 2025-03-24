# OS Patches metarepository

This repository contains all the sources for the elementary specific patches.

The packages are included into the [OS Patches Repository](https://launchpad.net/~elementary-os/+archive/ubuntu/os-patches).

## How is this repository working

The `master` branch is composed of GitHub Workflows that are running daily to
check for any new version in the Ubuntu repositories of the patched components.
If a newer version of a package is found in the Ubuntu repositories, a Pull Request
will be opened in this repository to let the team know to rebase patches and push
and update.

The workflow uses the `get-latest-version.py` python script included.

It depends on `python3-launchpadlib`, `python3-apt` and `python3-github`.

## Many branches

The repository is made of several distinct branches:
 * `import-list-$UBUNTU_NAME` is the branch listing the different packages
 that are getting patched.
 * `$PACKAGE-$UBUNTU_NAME` is the last source that got used to create the
 patched version
 * `$PACKAGE-$UBUNTU_NAME-patched` is `$PACKAGE-$UBUNTU_NAME` with the patch
 applied on it

The package list file in `import-list-$UBUNTU_NAME` contains one package per
line of packages to be monitored by the workflow. If the patched package has
been backported from a newer Ubuntu release, you can denote this in the list
with a colon separator

e.g. `packagekit:$NEWER_UBUNTU_NAME`

> Note that when possible, we try to discourage the use of OS patches and work
directly with upstream to include them.

## Creating a new Patch

1. Create a new orphan branch

    `git checkout --orphan {pkg-name}-{dist} && git rm -rf .`
  
2. Get the source of the package to be patched with `apt-source`. Do not use `sudo`.

    `apt source {pkg-name}`
  
3. Remove everything but the unpacked sources

    `rm *.tar.* *.dsc`
  
4. Move the source folder contents up to the pwd and then delete the empty folder

    `cp -r {source-folder-name}/. . && rm -r {source-folder-name}`
  
5. Add source files, commit, and push

    `git add * && git commit -am "Initial Import, version {pkg-version}"`
  
6. Create a `-patched` branch as a fork of this branch, not an orphan, make changes and push
7. Create a packaging recipe on Launchpad: https://code.launchpad.net/~elementary-os/elementaryos/+git/os-patches/+recipes
8. Once the build succeeds and is uploaded, update the `import-list-{dist}` branch. If this is done before the build succeeds, there will be false issue reports filed due to missing packages.

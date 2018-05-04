# package-importer
Keeps package source code branches up-to-date by checking for the existence of a git branch for a given package in a list and attempts to commit the new version if found after verifying its integrity.

## Usage
`GIT_WEB_URL="https://github.com/elementary/os-patches" ./package-importer import-list-bionic`

Note that `git` operations are performed using `ssh://` protocol magically determined from `GIT_WEB_URL`. `GIT_WEB_URL` is otherwise only used in human-friendly output messages.

## Lists
A list branch consists of a directory containing both a `sources.list` and a `packages_to_import` file containing package names. Only one package name per line.

## Automation
`package-importer` can be used in combination with systemd and [deploy keys](https://developer.github.com/guides/managing-deploy-keys).

## Variables
The following variables can be passed:
* `BASE_TMP_DIR` - Default: `$PWD`
* `GIT_WEB_URL` - Default: `https://github.com/elementary/os-patches`
* `GPG_KEYRING` - Default: `/etc/apt/trusted.gpg`

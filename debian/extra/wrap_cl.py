# Simple changelog entry formatter
#
# It simply uses the built in formatter and linewraps the text
#
# Use git-dch --customizations=/usr/share/doc/git-buildpackage/examples/wrap_cl.py
# or set it via gbp.conf

import gbp.dch
import os
from itertools import chain

def format_changelog_entry(commit_info, options, last_commit=False):
    entry = gbp.dch.format_changelog_entry(commit_info, options, last_commit)
    if not entry:
        return
    if commit_info['author'].name != os.environ['DEBFULLNAME']:
        entry.append(f"Author: {commit_info['author'].name}")

    cfs = [i.decode() for i in chain.from_iterable(commit_info['files'].values()) if i != b'debian/patches/series']
    if len(cfs) == 1:
        entry.append(f"File: {cfs[0]}")
    else:
        entry.append("Files:")
        [entry.append(f"- {i}") for i in cfs]
    entry.append(f"https://git.launchpad.net/~ubuntu-core-dev/ubuntu/+source/systemd/commit/?id={commit_info['id']}")
    return entry

#!/usr/bin/env python3

import os
import subprocess
import sys
import tarfile
import urllib.request

import apt_pkg
from debian import deb822
from github import Github
from launchpadlib.launchpad import Launchpad

DEFAULT_SERIES_NAME = "noble"

# Process the command line arguments
if len(sys.argv) < 2:
    raise ValueError("Please provide a package name")

if len(sys.argv) < 3:
    series_name = DEFAULT_SERIES_NAME
else:
    series_name = sys.argv[2]

if not series_name:
    series_name = DEFAULT_SERIES_NAME

if len(sys.argv) < 4:
    upstream_series_name = series_name
else:
    upstream_series_name = sys.argv[3]

if not upstream_series_name:
    upstream_series_name = series_name

component_name = sys.argv[1]

# Initialize APT
apt_pkg.init_system()

# Initialize Launchpad variables
launchpad = Launchpad.login_anonymously(
    "elementary daily test",
    "production",
    "~/.launchpadlib/cache/",
    version="devel"
)

ubuntu = launchpad.distributions["ubuntu"]
ubuntu_archive = ubuntu.main_archive
patches_archive = launchpad.people["elementary-os"].getPPAByName(
    distribution=ubuntu, name="os-patches"
)
series = ubuntu.getSeries(name_or_version=series_name)
upstream_series = ubuntu.getSeries(name_or_version=upstream_series_name)

# Initialize GitHub variables
github_token = os.environ["GITHUB_TOKEN"]
github_repo = os.environ["GITHUB_REPOSITORY"]
github = Github(github_token)
repo = github.get_repo(github_repo)

subprocess.run(["git", "config", "--global", "user.email", "github-actions[bot]@users.noreply.github.com"], check=False)
subprocess.run(["git", "config", "--global", "user.name", "github-actions[bot]"], check=False)
subprocess.run(["git", "config", "--global", "--add", "safe.directory", "/__w/os-patches/os-patches"], check=False)


def github_issue_exists(title):
    """Method for checking if GitHub Actions has already opened an issue with this title"""
    open_issues = repo.get_issues(state="open")
    for open_issue in open_issues:
        if open_issue.title == title and open_issue.user.login == "github-actions[bot]":
            return True
    return False

def get_patched_sources():
    """Get the current version of a package in elementary os patches PPA"""
    return patches_archive.getPublishedSources(
        exact_match=True,
        source_name=component_name,
        status="Published",
        distro_series=series,
    )

def get_upstream_sources():
    """Get the current version of a package in upstream PPA"""
    return ubuntu_archive.getPublishedSources(
        exact_match=True,
        source_name=component_name,
        status="Published",
        pocket=pocket,
        distro_series=upstream_series,
    )


if len(get_patched_sources()) == 0:
    issue_title = f"Package {component_name} not found in os-patches PPA"
    if not github_issue_exists(issue_title):
        issue = repo.create_issue(
            issue_title,
            f"{component_name} found in the import list, but not in the PPA. Not deployed yet or removed by accident?",
        )
        print(f"Package {component_name} not found in elementary os-patches! - Created issue {issue.number}")
    sys.exit(0)

patched_version = get_patched_sources()[0].source_package_version

# Search for a new version in the Ubuntu repositories
for pocket in ["Release", "Security", "Updates"]:
    upstream_sources = get_upstream_sources()
    if len(upstream_sources) > 0:
        pocket_version = upstream_sources[0].source_package_version
        if apt_pkg.version_compare(pocket_version, patched_version) > 0:
            issue_title = f"📦 New version of {component_name} available [{upstream_series_name}]"
            if not github_issue_exists(issue_title):
                issue = repo.create_issue(
                    issue_title,
                    f"""The package `{component_name}` in `{upstream_series_name}` can be upgraded """
                    f"""to version `{pocket_version}`.\nPrevious version: `{patched_version}`."""
                )
                print(
                    f"""The patched package {component_name} has a new version {pocket_version}"""
                    f"""(was version {patched_version}) - Created issue {issue.number}"""
                )


                base_branch = f"{component_name}-{upstream_series_name}"
                new_branch = f"bot/update/{component_name}-{upstream_series_name}"

                subprocess.run(["git", "fetch", "--all"], check=True)
                subprocess.run(["git", "switch", base_branch], check=True)

                subprocess.run(["git", "checkout", "-b", new_branch], check=True)

                subprocess.run(["sed", "-i", "'s/^#\s*\(deb-src.*\)/\1/'", "/etc/apt/sources.list"], check=True)

                subprocess.run(["apt", "source", component_name], check=True)
                subprocess.run(["rm", "*.tar.*", "*.dsc"], check=True)

                subprocess.run(["cp", "-r", f"{component_name}.*/*", "."], check=True)
                subprocess.run(["rm", "-r", f"{component_name}.*"], check=True)

                # Add all changes
                subprocess.run(["git", "add", "."], check=True)
                # Commit the changes
                subprocess.run(["git", "commit", "-m", f"Update to {component_name} {pocket_version}"], check=True)
                # Push the new branch to the remote repository
                subprocess.run(["git", "push", "origin", new_branch], check=True)
                pr = repo.create_pull(
                    base=base_branch,
                    head=new_branch,
                    title=f"📦 Update {component_name}",
                    body=f"""A new version of `{component_name} {pocket_version}` replaces `{patched_version}`.
                    Fixes #{issue.number}."""
                )
                subprocess.run(["git", "switch", "master"], check=True)

#!/usr/bin/env python3

import os
import subprocess
import sys
import apt_pkg
import git
import git.config
from github import Github
from launchpadlib.launchpad import Launchpad

DEFAULT_SERIES_NAME = "noble"


def get_packages_list() -> list:
    with open("/tmp/patched-packages", "r", encoding="utf-8") as file:
        items = file.read().splitlines()
    return items


def github_pull_exists(title, repo):
    """Check if GitHub Actions has already opened a PR with this title"""
    open_pulls = repo.get_pulls(state="open")
    for open_pull in open_pulls:
        if open_pull.title == title and open_pull.user.login == "github-actions[bot]":
            return True
    return False


def main():
    series_name = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SERIES_NAME

    # Configuring this repo to be able to commit as a bot
    current_repo = git.Repo(".")
    with current_repo.config_writer(config_level="global") as git_config:
        git_config.set_value(
            "user", "email", "github-actions[bot]@users.noreply.github.com"
        )
        git_config.set_value("user", "name", "github-actions[bot]")
        git_config.set_value("checkout", "defaultRemote", "origin")
        git_config.add_value("safe", "directory", "/__w/os-patches/os-patches")

    current_repo.git.fetch("--all")

    github_token = os.environ["GITHUB_TOKEN"]
    github_repo = os.environ["GITHUB_REPOSITORY"]
    github = Github(github_token)
    repo = github.get_repo(github_repo)

    # Initialize APT
    apt_pkg.init_system()

    # Initialize Launchpad variables
    launchpad = Launchpad.login_anonymously(
        "elementary daily test", "production", "~/.launchpadlib/cache/", version="devel"
    )

    ubuntu = launchpad.distributions["ubuntu"]
    ubuntu_archive = ubuntu.main_archive
    patches_archive = launchpad.people["elementary-os"].getPPAByName(
        distribution=ubuntu, name="os-patches"
    )

    packages_and_upstream = get_packages_list()
    series = ubuntu.getSeries(name_or_version=series_name)

    for package_and_upstream in packages_and_upstream:
        package_name, *upstream_series_name = package_and_upstream.split(":", 1)
        upstream_series_name = (
            upstream_series_name[0] if upstream_series_name else series_name
        )
        print(package_name, upstream_series_name)
        
        upstream_series = ubuntu.getSeries(name_or_version=upstream_series_name)

        patched_sources = patches_archive.getPublishedSources(
            exact_match=True,
            source_name=package_name,
            status="Published",
            distro_series=series,
        )
        patched_version = patched_sources[0].source_package_version

        # Search for a new version in the Ubuntu repositories
        for pocket in ["Release", "Security", "Updates"]:

            upstream_sources = ubuntu_archive.getPublishedSources(
                exact_match=True,
                source_name=package_name,
                status="Published",
                pocket=pocket,
                distro_series=upstream_series,
            )

            if len(upstream_sources) <= 0:
                continue

            pocket_version = upstream_sources[0].source_package_version
            if apt_pkg.version_compare(pocket_version, patched_version) <= 0:
                continue

            pull_title = f"📦 Update {package_name} [{upstream_series_name}]"
            if github_pull_exists(pull_title, repo):
                continue

            base_branch = f"{package_name}-{upstream_series_name}"
            new_branch = f"bot/update/{package_name}-{upstream_series_name}"

            # Checkout the base branch
            current_repo.git.checkout(base_branch)
            # Create and checkout the new branch
            current_repo.git.checkout("-b", new_branch)

            p_apt_source = subprocess.run(
                f"apt source {package_name}",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
                encoding="utf-8",
            )

            # getting the directory to which the source is extracted
            extraction_dest = ""
            for line in p_apt_source.stdout.split("\n"):
                if "dpkg-source: info: extracting" in line:
                    print(line)
                    extraction_dest = line.split()[-1]

            subprocess.run(
                "rm *.tar.* *.dsc",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            )
            subprocess.run(
                f"cp -r {extraction_dest}/* .",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            )

            subprocess.run(
                f"rm -r {extraction_dest}",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            )

            current_repo.git.add(A=True)
            current_repo.index.commit(f"Update to {package_name} {pocket_version}")
            current_repo.git.push("origin", new_branch)
            pr = repo.create_pull(
                base=base_branch,
                head=new_branch,
                title=pull_title,
                body=f"""A new version of `{package_name} {pocket_version}` replaces `{patched_version}`.""",
            )
            current_repo.git.checkout("master")


if __name__ == "__main__":
    main()

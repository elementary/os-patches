#!/usr/bin/python3
# Builds on python2.X and python3
# $Id: setup.py,v 1.2 2002/01/08 07:13:21 jgg Exp $
import glob
import os
import shutil
import subprocess
import sys

from setuptools import Extension, setup
from setuptools.command.install import install

cmdclass = {}

try:
    from DistUtilsExtra.auto import clean_build_tree
    from DistUtilsExtra.command import build_extra, build_i18n
except ImportError:
    print("W: [python%s] DistUtilsExtra import error." % sys.version[:3])
else:
    cmdclass["build"] = build_extra.build_extra
    cmdclass["build_i18n"] = build_i18n.build_i18n
    cmdclass["clean"] = clean_build_tree


class InstallTypeinfo(install):
    def run(self):
        install.run(self)
        for pyi in glob.glob("typehinting/*.pyi"):
            stubs = os.path.basename(pyi).split(".")[0] + "-stubs"
            stubs = os.path.join(self.install_purelib, stubs)
            if not os.path.exists(stubs):
                os.makedirs(stubs)
            shutil.copy(pyi, os.path.join(stubs, "__init__.pyi"))


cmdclass["install"] = InstallTypeinfo


def get_version():
    """Get a PEP 0440 compatible version string"""
    version = os.environ.get("DEBVER")
    if not version:
        proc = subprocess.Popen(
            ["dpkg-parsechangelog", "-SVersion"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        out, _ = proc.communicate()
        if proc.returncode == 0:
            version = out.decode("utf-8")

    version = version.strip()

    if not version:
        return version

    version = version.replace("~alpha", ".a")
    version = version.replace("~beta", ".b")
    version = version.replace("~rc", ".rc")
    version = version.replace("~exp", ".dev")
    version = version.replace("ubuntu", "+ubuntu")
    version = version.replace("tanglu", "+tanglu")
    version = version.split("build")[0]

    return version


# The apt_pkg module.
files = [
    "apt_pkgmodule.cc",
    "acquire.cc",
    "cache.cc",
    "cdrom.cc",
    "configuration.cc",
    "depcache.cc",
    "generic.cc",
    "hashes.cc",
    "hashstring.cc",
    "indexfile.cc",
    "metaindex.cc",
    "pkgmanager.cc",
    "pkgrecords.cc",
    "pkgsrcrecords.cc",
    "policy.cc",
    "progress.cc",
    "sourcelist.cc",
    "string.cc",
    "tag.cc",
    "lock.cc",
    "acquire-item.cc",
    "python-apt-helpers.cc",
    "cachegroup.cc",
    "orderlist.cc",
    "hashstringlist.cc",
]
files = sorted(["python/" + fname for fname in files], key=lambda s: s[:-3])
apt_pkg = Extension(
    "apt_pkg",
    files,
    libraries=["apt-pkg"],
    extra_compile_args=[
        "-std=c++11",
        "-Wno-write-strings",
        "-DAPT_8_CLEANER_HEADERS",
        "-DAPT_9_CLEANER_HEADERS",
        "-DAPT_10_CLEANER_HEADERS",
        "-DPY_SSIZE_T_CLEAN",
    ],
)

# The apt_inst module
files = [
    "python/apt_instmodule.cc",
    "python/generic.cc",
    "python/arfile.cc",
    "python/tarfile.cc",
]
apt_inst = Extension(
    "apt_inst",
    files,
    libraries=["apt-pkg"],
    extra_compile_args=["-std=c++11", "-Wno-write-strings", "-DPY_SSIZE_T_CLEAN"],
)

# Replace the leading _ that is used in the templates for translation
if len(sys.argv) > 1 and sys.argv[1] == "build":
    if not os.path.exists("build/data/templates/"):
        os.makedirs("build/data/templates")
    for template in glob.glob("data/templates/*.info.in"):
        source = open(template)
        build = open("build/" + template[:-3], "w")
        for line in source:
            build.write(line.lstrip("_"))
        source.close()
        build.close()
    for template in glob.glob("data/templates/*.mirrors"):
        shutil.copy(template, os.path.join("build", template))


setup(
    name="python-apt",
    description="Python bindings for APT",
    version=get_version(),
    author="APT Development Team",
    author_email="deity@lists.debian.org",
    ext_modules=[apt_pkg, apt_inst],
    packages=["apt", "apt.progress", "aptsources"],
    package_data={
        "apt": ["*.pyi", "py.typed"],
    },
    data_files=[
        ("share/python-apt/templates", glob.glob("build/data/templates/*.info")),
        ("share/python-apt/templates", glob.glob("data/templates/*.mirrors")),
    ],
    cmdclass=cmdclass,
    license="GNU GPL",
    platforms="posix",
)

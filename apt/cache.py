# cache.py - apt cache abstraction
#
#  Copyright (c) 2005-2009 Canonical
#
#  Author: Michael Vogt <michael.vogt@ubuntu.com>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA

from __future__ import print_function

import fnmatch
import os
import warnings
import weakref


from typing import (Any, Callable, Dict, Iterator, List, Optional,
                    Set, Tuple, Union, cast, KeysView)

import apt_pkg
from apt.package import Package, Version
import apt.progress.text
from apt.progress.base import AcquireProgress, InstallProgress, OpProgress


class FetchCancelledException(IOError):
    """Exception that is thrown when the user cancels a fetch operation."""


class FetchFailedException(IOError):
    """Exception that is thrown when fetching fails."""


class UntrustedException(FetchFailedException):
    """Exception that is thrown when fetching fails for trust reasons"""


class LockFailedException(IOError):
    """Exception that is thrown when locking fails."""


class CacheClosedException(Exception):
    """Exception that is thrown when the cache is used after close()."""


class _WrappedLock(object):
    """Wraps an apt_pkg.FileLock to raise LockFailedException.

    Initialized using a directory path."""

    def __init__(self, path):
        # type: (str) -> None
        self._path = path
        self._lock = apt_pkg.FileLock(os.path.join(path, "lock"))

    def __enter__(self):
        # type: () -> None
        try:
            return self._lock.__enter__()
        except apt_pkg.Error as e:
            raise LockFailedException(("Failed to lock directory %s: %s") %
                                       (self._path, e))

    def __exit__(self, typ, value, traceback):
        # type: (object, object, object) -> None
        return self._lock.__exit__(typ, value, traceback)


class Cache(object):
    """Dictionary-like package cache.

    The APT cache file contains a hash table mapping names of binary
    packages to their metadata. A Cache object is the in-core
    representation of the same. It provides access to APTs idea of the
    list of available packages.

    The cache can be used like a mapping from package names to Package
    objects (although only getting items is supported).

    Keyword arguments:
    progress -- a OpProgress object,
    rootdir  -- an alternative root directory. if that is given the system
    sources.list and system lists/files are not read, only file relative
    to the given rootdir,
    memonly  -- build the cache in memory only.


    .. versionchanged:: 1.0

        The cache now supports package names with special architecture
        qualifiers such as :all and :native. It does not export them
        in :meth:`keys()`, though, to keep :meth:`keys()` a unique set.
    """

    def __init__(self, progress=None, rootdir=None, memonly=False):
        # type: (Optional[OpProgress], Optional[str], bool) -> None
        self._cache = cast(apt_pkg.Cache, None)  # type: apt_pkg.Cache
        self._depcache = cast(apt_pkg.DepCache, None)  # type: apt_pkg.DepCache
        self._records = cast(apt_pkg.PackageRecords, None)  # type: apt_pkg.PackageRecords # noqa
        self._list = cast(apt_pkg.SourceList, None)  # type: apt_pkg.SourceList
        self._callbacks = {}  # type: Dict[str, List[Union[Callable[..., None],str]]] # noqa
        self._callbacks2 = {}  # type: Dict[str, List[Tuple[Callable[..., Any], Tuple[Any, ...], Dict[Any,Any]]]] # noqa
        self._weakref = weakref.WeakValueDictionary()  # type: weakref.WeakValueDictionary[str, apt.Package] # noqa
        self._weakversions = weakref.WeakSet()  # type: weakref.WeakSet[Version] # noqa
        self._changes_count = -1
        self._sorted_set = None  # type: Optional[List[str]]

        self.connect("cache_post_open", "_inc_changes_count")
        self.connect("cache_post_change", "_inc_changes_count")
        if memonly:
            # force apt to build its caches in memory
            apt_pkg.config.set("Dir::Cache::pkgcache", "")
        if rootdir:
            rootdir = os.path.abspath(rootdir)
            if os.path.exists(rootdir + "/etc/apt/apt.conf"):
                apt_pkg.read_config_file(apt_pkg.config,
                                         rootdir + "/etc/apt/apt.conf")
            if os.path.isdir(rootdir + "/etc/apt/apt.conf.d"):
                apt_pkg.read_config_dir(apt_pkg.config,
                                        rootdir + "/etc/apt/apt.conf.d")
            apt_pkg.config.set("Dir", rootdir)
            apt_pkg.config.set("Dir::State::status",
                               rootdir + "/var/lib/dpkg/status")
            # also set dpkg to the rootdir path so that its called for the
            # --print-foreign-architectures call
            apt_pkg.config.set("Dir::bin::dpkg",
                               os.path.join(rootdir, "usr", "bin", "dpkg"))
            # create required dirs/files when run with special rootdir
            # automatically
            self._check_and_create_required_dirs(rootdir)
            # Call InitSystem so the change to Dir::State::Status is actually
            # recognized (LP: #320665)
            apt_pkg.init_system()

        # Prepare a lock object (context manager for archive lock)
        archive_dir = apt_pkg.config.find_dir("Dir::Cache::Archives")
        self._archive_lock = _WrappedLock(archive_dir)

        self.open(progress)

    def fix_broken(self):
        # type: () -> None
        """Fix broken packages."""
        self._depcache.fix_broken()

    def _inc_changes_count(self):
        # type: () -> None
        """Increase the number of changes"""
        self._changes_count += 1

    def _check_and_create_required_dirs(self, rootdir):
        # type: (str) -> None
        """
        check if the required apt directories/files are there and if
        not create them
        """
        files = [
            "/var/lib/dpkg/status",
            "/etc/apt/sources.list",
        ]
        dirs = [
            "/var/lib/dpkg",
            "/etc/apt/",
            "/var/cache/apt/archives/partial",
            "/var/lib/apt/lists/partial",
        ]
        for d in dirs:
            if not os.path.exists(rootdir + d):
                #print "creating: ", rootdir + d
                os.makedirs(rootdir + d)
        for f in files:
            if not os.path.exists(rootdir + f):
                open(rootdir + f, "w").close()

    def _run_callbacks(self, name):
        # type: (str) -> None
        """ internal helper to run a callback """
        if name in self._callbacks:
            for callback in self._callbacks[name]:
                if callback == '_inc_changes_count':
                    self._inc_changes_count()
                else:
                    callback()  # type: ignore

        if name in self._callbacks2:
            for callback, args, kwds in self._callbacks2[name]:
                callback(self, *args, **kwds)

    def open(self, progress=None):
        # type: (Optional[OpProgress]) -> None
        """ Open the package cache, after that it can be used like
            a dictionary
        """
        if progress is None:
            progress = apt.progress.base.OpProgress()
        # close old cache on (re)open
        self.close()
        self.op_progress = progress
        self._run_callbacks("cache_pre_open")

        self._cache = apt_pkg.Cache(progress)
        self._depcache = apt_pkg.DepCache(self._cache)
        self._records = apt_pkg.PackageRecords(self._cache)
        self._list = apt_pkg.SourceList()
        self._list.read_main_list()
        self._sorted_set = None
        self.__remap()

        self._have_multi_arch = len(apt_pkg.get_architectures()) > 1

        progress.done()
        self._run_callbacks("cache_post_open")

    def __remap(self):
        # type: () -> None
        """Called after cache reopen() to relocate to new cache.

        Relocate objects like packages and versions from the old
        underlying cache to the new one.
        """
        for key in list(self._weakref.keys()):
            try:
                pkg = self._weakref[key]
            except KeyError:
                continue

            try:
                pkg._pkg = self._cache[pkg._pkg.name, pkg._pkg.architecture]
            except LookupError:
                del self._weakref[key]

        for ver in list(self._weakversions):
            # Package has been reseated above, reseat version
            for v in ver.package._pkg.version_list:
                # Requirements as in debListParser::SameVersion
                if (v.hash == ver._cand.hash and
                    (v.size == 0 or ver._cand.size == 0 or
                     v.size == ver._cand.size) and
                    v.multi_arch == ver._cand.multi_arch and
                    v.ver_str == ver._cand.ver_str):
                    ver._cand = v
                    break
            else:
                self._weakversions.remove(ver)

    def close(self):
        # type: () -> None
        """ Close the package cache """
        # explicitely free the FDs that _records has open
        del self._records
        self._records = cast(apt_pkg.PackageRecords, None)

    def __enter__(self):
        # type: () -> Cache
        """ Enter the with statement """
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # type: (object, object, object) -> None
        """ Exit the with statement """
        self.close()

    def __getitem__(self, key):
        # type: (object) -> Package
        """ look like a dictionary (get key) """
        try:
            key = str(key)
            rawpkg = self._cache[key]
        except KeyError:
            raise KeyError('The cache has no package named %r' % key)

        # It might be excluded due to not having a version or something
        if not self.__is_real_pkg(rawpkg):
            raise KeyError('The cache has no package named %r' % key)

        pkg = self._rawpkg_to_pkg(rawpkg)

        return pkg

    def get(self, key, default=None):
        # type: (object, object) -> Any
        """Return *self*[*key*] or *default* if *key* not in *self*.

        .. versionadded:: 1.1
        """
        try:
            return self[key]
        except KeyError:
            return default

    def _rawpkg_to_pkg(self, rawpkg):
        # type: (apt_pkg.Package) -> Package
        """Returns the apt.Package object for an apt_pkg.Package object.

        .. versionadded:: 1.0.0
        """
        fullname = rawpkg.get_fullname(pretty=True)

        return self._weakref.setdefault(fullname, Package(self, rawpkg))

    def __iter__(self):
        # type: () -> Iterator[Package]
        # We iterate sorted over package names here. With this we read the
        # package lists linearly if we need to access the package records,
        # instead of having to do thousands of random seeks; the latter
        # is disastrous if we use compressed package indexes, and slower than
        # necessary for uncompressed indexes.
        for pkgname in self.keys():
            pkg = Package(self, self._cache[pkgname])
            yield self._weakref.setdefault(pkgname, pkg)

    def __is_real_pkg(self, rawpkg):
        # type: (apt_pkg.Package) -> bool
        """Check if the apt_pkg.Package provided is a real package."""
        return rawpkg.has_versions

    def has_key(self, key):
        # type: (object) -> bool
        return key in self

    def __contains__(self, key):
        # type: (object) -> bool
        try:
            return self.__is_real_pkg(self._cache[str(key)])
        except KeyError:
            return False

    def __len__(self):
        # type: () -> int
        return len(self.keys())

    def keys(self):
        # type: () -> List[str]
        if self._sorted_set is None:
            self._sorted_set = sorted(p.get_fullname(pretty=True)
                                      for p in self._cache.packages
                                        if self.__is_real_pkg(p))
        return list(self._sorted_set)  # We need a copy here, caller may modify

    def get_changes(self):
        # type: () -> List[Package]
        """ Get the marked changes """
        changes = []
        marked_keep = self._depcache.marked_keep
        for rawpkg in self._cache.packages:
            if not marked_keep(rawpkg):
                changes.append(self._rawpkg_to_pkg(rawpkg))
        return changes

    def upgrade(self, dist_upgrade=False):
        # type: (bool) -> None
        """Upgrade all packages.

        If the parameter *dist_upgrade* is True, new dependencies will be
        installed as well (and conflicting packages may be removed). The
        default value is False.
        """
        self.cache_pre_change()
        self._depcache.upgrade(dist_upgrade)
        self.cache_post_change()

    @property
    def required_download(self):
        # type: () -> int
        """Get the size of the packages that are required to download."""
        if self._records is None:
            raise CacheClosedException(
                "Cache object used after close() called")
        pm = apt_pkg.PackageManager(self._depcache)
        fetcher = apt_pkg.Acquire()
        pm.get_archives(fetcher, self._list, self._records)
        return fetcher.fetch_needed

    @property
    def required_space(self):
        # type: () -> int
        """Get the size of the additional required space on the fs."""
        return self._depcache.usr_size

    @property
    def req_reinstall_pkgs(self):
        # type: () -> Set[str]
        """Return the packages not downloadable packages in reqreinst state."""
        reqreinst = set()
        get_candidate_ver = self._depcache.get_candidate_ver
        states = frozenset((apt_pkg.INSTSTATE_REINSTREQ,
                            apt_pkg.INSTSTATE_HOLD_REINSTREQ))
        for pkg in self._cache.packages:
            cand = get_candidate_ver(pkg)
            if cand and not cand.downloadable and pkg.inst_state in states:
                reqreinst.add(pkg.get_fullname(pretty=True))
        return reqreinst

    def _run_fetcher(self, fetcher, allow_unauthenticated):
        # type: (apt_pkg.Acquire, Optional[bool]) -> int
        if allow_unauthenticated is None:
            allow_unauthenticated = apt_pkg.config.find_b("APT::Get::"
                                        "AllowUnauthenticated", False)

        untrusted = [item for item in fetcher.items if not item.is_trusted]
        if untrusted and not allow_unauthenticated:
            raise UntrustedException("Untrusted packages:\n%s" %
                "\n".join(i.desc_uri for i in untrusted))

        # do the actual fetching
        res = fetcher.run()

        # now check the result (this is the code from apt-get.cc)
        failed = False
        err_msg = ""
        for item in fetcher.items:
            if item.status == item.STAT_DONE:
                continue
            if item.STAT_IDLE:
                continue
            err_msg += "Failed to fetch %s %s\n" % (item.desc_uri,
                                                    item.error_text)
            failed = True

        # we raise a exception if the download failed or it was cancelt
        if res == fetcher.RESULT_CANCELLED:
            raise FetchCancelledException(err_msg)
        elif failed:
            raise FetchFailedException(err_msg)
        return res

    def _fetch_archives(self,
                        fetcher,                # type: apt_pkg.Acquire
                        pm,                     # type: apt_pkg.PackageManager
                        allow_unauthenticated=None,  # type: Optional[bool]
                        ):
        # type: (...) -> int
        """ fetch the needed archives """
        if self._records is None:
            raise CacheClosedException(
                "Cache object used after close() called")

        # this may as well throw a SystemError exception
        if not pm.get_archives(fetcher, self._list, self._records):
            return False

        # now run the fetcher, throw exception if something fails to be
        # fetched
        return self._run_fetcher(fetcher, allow_unauthenticated)

    def fetch_archives(self,
                       progress=None,        # type: Optional[AcquireProgress]
                       fetcher=None,         # type: Optional[apt_pkg.Acquire]
                       allow_unauthenticated=None,  # type: Optional[bool]
                       ):
        # type: (...) -> int
        """Fetch the archives for all packages marked for install/upgrade.

        You can specify either an :class:`apt.progress.base.AcquireProgress()`
        object for the parameter *progress*, or specify an already
        existing :class:`apt_pkg.Acquire` object for the parameter *fetcher*.

        The return value of the function is undefined. If an error occurred,
        an exception of type :class:`FetchFailedException` or
        :class:`FetchCancelledException` is raised.

        The keyword-only parameter *allow_unauthenticated* specifies whether
        to allow unauthenticated downloads. If not specified, it defaults to
        the configuration option `APT::Get::AllowUnauthenticated`.

        .. versionadded:: 0.8.0
        """
        if progress is not None and fetcher is not None:
            raise ValueError("Takes a progress or a an Acquire object")
        if progress is None:
            progress = apt.progress.text.AcquireProgress()
        if fetcher is None:
            fetcher = apt_pkg.Acquire(progress)

        with self._archive_lock:
            return self._fetch_archives(fetcher,
                                        apt_pkg.PackageManager(self._depcache),
                                        allow_unauthenticated)

    def is_virtual_package(self, pkgname):
        # type: (str) -> bool
        """Return whether the package is a virtual package."""
        try:
            pkg = self._cache[pkgname]
        except KeyError:
            return False
        else:
            return bool(pkg.has_provides and not pkg.has_versions)

    def get_providing_packages(self, pkgname, candidate_only=True,
                               include_nonvirtual=False):
        # type: (str, bool, bool) -> List[Package]
        """Return a list of all packages providing a package.

        Return a list of packages which provide the virtual package of the
        specified name.

        If 'candidate_only' is False, return all packages with at
        least one version providing the virtual package. Otherwise,
        return only those packages where the candidate version
        provides the virtual package.

        If 'include_nonvirtual' is True then it will search for all
        packages providing pkgname, even if pkgname is not itself
        a virtual pkg.
        """

        providers = set()  # type: Set[Package]
        get_candidate_ver = self._depcache.get_candidate_ver
        try:
            vp = self._cache[pkgname]
            if vp.has_versions and not include_nonvirtual:
                return list(providers)
        except KeyError:
            return list(providers)

        for provides, providesver, version in vp.provides_list:
            rawpkg = version.parent_pkg
            if not candidate_only or (version == get_candidate_ver(rawpkg)):
                providers.add(self._rawpkg_to_pkg(rawpkg))
        return list(providers)

    def update(self, fetch_progress=None, pulse_interval=0,
               raise_on_error=True, sources_list=None):
        # type: (Optional[AcquireProgress], int, bool, Optional[str]) -> int
        """Run the equivalent of apt-get update.

        You probably want to call open() afterwards, in order to utilise the
        new cache. Otherwise, the old cache will be used which can lead to
        strange bugs.

        The first parameter *fetch_progress* may be set to an instance of
        apt.progress.FetchProgress, the default is apt.progress.FetchProgress()
        .
        sources_list -- Update a alternative sources.list than the default.
        Note that the sources.list.d directory is ignored in this case
        """
        with _WrappedLock(apt_pkg.config.find_dir("Dir::State::Lists")):
            if sources_list:
                old_sources_list = apt_pkg.config.find("Dir::Etc::sourcelist")
                old_sources_list_d = (
                    apt_pkg.config.find("Dir::Etc::sourceparts"))
                old_cleanup = apt_pkg.config.find("APT::List-Cleanup")
                apt_pkg.config.set("Dir::Etc::sourcelist",
                                   os.path.abspath(sources_list))
                apt_pkg.config.set("Dir::Etc::sourceparts", "xxx")
                apt_pkg.config.set("APT::List-Cleanup", "0")
                slist = apt_pkg.SourceList()
                slist.read_main_list()
            else:
                slist = self._list

            try:
                if fetch_progress is None:
                    fetch_progress = apt.progress.base.AcquireProgress()
                try:
                    res = self._cache.update(fetch_progress, slist,
                                             pulse_interval)
                except SystemError as e:
                    raise FetchFailedException(e)
                if not res and raise_on_error:
                    raise FetchFailedException()
                else:
                    return res
            finally:
                if sources_list:
                    apt_pkg.config.set("Dir::Etc::sourcelist",
                                       old_sources_list)
                    apt_pkg.config.set("Dir::Etc::sourceparts",
                                       old_sources_list_d)
                    apt_pkg.config.set("APT::List-Cleanup",
                                       old_cleanup)

    def install_archives(self, pm, install_progress):
        # type: (apt_pkg.PackageManager, InstallProgress) -> int
        """
        The first parameter *pm* refers to an object returned by
        apt_pkg.PackageManager().

        The second parameter *install_progress* refers to an InstallProgress()
        object of the module apt.progress.

        This releases a system lock in newer versions, if there is any,
        and reestablishes it afterwards.
        """
        # compat with older API
        try:
            install_progress.startUpdate()  # type: ignore
        except AttributeError:
            install_progress.start_update()

        did_unlock = apt_pkg.pkgsystem_is_locked()
        if did_unlock:
            apt_pkg.pkgsystem_unlock_inner()

        try:
            res = install_progress.run(pm)
        finally:
            if did_unlock:
                apt_pkg.pkgsystem_lock_inner()

        try:
            install_progress.finishUpdate()  # type: ignore
        except AttributeError:
            install_progress.finish_update()
        return res

    def commit(self,
               fetch_progress=None,         # type: Optional[AcquireProgress]
               install_progress=None,       # type: Optional[InstallProgress]
               allow_unauthenticated=None,  # type: Optional[bool]
               ):
        # type: (...) -> bool
        """Apply the marked changes to the cache.

        The first parameter, *fetch_progress*, refers to a FetchProgress()
        object as found in apt.progress, the default being
        apt.progress.FetchProgress().

        The second parameter, *install_progress*, is a
        apt.progress.InstallProgress() object.

        The keyword-only parameter *allow_unauthenticated* specifies whether
        to allow unauthenticated downloads. If not specified, it defaults to
        the configuration option `APT::Get::AllowUnauthenticated`.
        """
        # FIXME:
        # use the new acquire/pkgmanager interface here,
        # raise exceptions when a download or install fails
        # and send proper error strings to the application.
        # Current a failed download will just display "error"
        # which is less than optimal!

        if fetch_progress is None:
            fetch_progress = apt.progress.base.AcquireProgress()
        if install_progress is None:
            install_progress = apt.progress.base.InstallProgress()

        assert install_progress is not None

        with apt_pkg.SystemLock():
            pm = apt_pkg.PackageManager(self._depcache)
            fetcher = apt_pkg.Acquire(fetch_progress)
            with self._archive_lock:
                while True:
                    # fetch archives first
                    res = self._fetch_archives(fetcher, pm,
                                               allow_unauthenticated)

                    # then install
                    res = self.install_archives(pm, install_progress)
                    if res == pm.RESULT_COMPLETED:
                        break
                    elif res == pm.RESULT_FAILED:
                        raise SystemError("installArchives() failed")
                    elif res == pm.RESULT_INCOMPLETE:
                        pass
                    else:
                        raise SystemError("internal-error: unknown result "
                                          "code from InstallArchives: %s" %
                                          res)
                    # reload the fetcher for media swaping
                    fetcher.shutdown()
        return (res == pm.RESULT_COMPLETED)

    def clear(self):
        # type: () -> None
        """ Unmark all changes """
        self._depcache.init()

    # cache changes

    def cache_post_change(self):
        # type: () -> None
        " called internally if the cache has changed, emit a signal then "
        self._run_callbacks("cache_post_change")

    def cache_pre_change(self):
        # type: () -> None
        """ called internally if the cache is about to change, emit
            a signal then """
        self._run_callbacks("cache_pre_change")

    def connect(self, name, callback):
        # type: (str, Union[Callable[..., None],str]) -> None
        """Connect to a signal.

        .. deprecated:: 1.0

            Please use connect2() instead, as this function is very
            likely to cause a memory leak.
        """
        if callback != '_inc_changes_count':
            warnings.warn("connect() likely causes a reference"
                          " cycle, use connect2() instead", RuntimeWarning, 2)
        if name not in self._callbacks:
            self._callbacks[name] = []
        self._callbacks[name].append(callback)

    def connect2(self, name, callback, *args, **kwds):
        # type: (str, Callable[..., Any], object, object) -> None
        """Connect to a signal.

        The callback will be passed the cache as an argument, and
        any arguments passed to this function. Make sure that, if you
        pass a method of a class as your callback, your class does not
        contain a reference to the cache.

        Cyclic references to the cache can cause issues if the Cache object
        is replaced by a new one, because the cache keeps a lot of objects and
        tens of open file descriptors.

        currently only used for cache_{post,pre}_{changed,open}.

        .. versionadded:: 1.0
        """
        if name not in self._callbacks2:
            self._callbacks2[name] = []
        self._callbacks2[name].append((callback, args, kwds))

    def actiongroup(self):
        # type: () -> apt_pkg.ActionGroup
        """Return an `ActionGroup` object for the current cache.

        Action groups can be used to speedup actions. The action group is
        active as soon as it is created, and disabled when the object is
        deleted or when release() is called.

        You can use the action group as a context manager, this is the
        recommended way::

            with cache.actiongroup():
                for package in my_selected_packages:
                    package.mark_install()

        This way, the action group is automatically released as soon as the
        with statement block is left. It also has the benefit of making it
        clear which parts of the code run with a action group and which
        don't.
        """
        return apt_pkg.ActionGroup(self._depcache)

    @property
    def dpkg_journal_dirty(self):
        # type: () -> bool
        """Return True if the dpkg was interrupted

        All dpkg operations will fail until this is fixed, the action to
        fix the system if dpkg got interrupted is to run
        'dpkg --configure -a' as root.
        """
        dpkg_status_dir = os.path.dirname(
            apt_pkg.config.find_file("Dir::State::status"))
        for f in os.listdir(os.path.join(dpkg_status_dir, "updates")):
            if fnmatch.fnmatch(f, "[0-9]*"):
                return True
        return False

    @property
    def broken_count(self):
        # type: () -> int
        """Return the number of packages with broken dependencies."""
        return self._depcache.broken_count

    @property
    def delete_count(self):
        # type: () -> int
        """Return the number of packages marked for deletion."""
        return self._depcache.del_count

    @property
    def install_count(self):
        # type: () -> int
        """Return the number of packages marked for installation."""
        return self._depcache.inst_count

    @property
    def keep_count(self):
        # type: () -> int
        """Return the number of packages marked as keep."""
        return self._depcache.keep_count


class ProblemResolver(object):
    """Resolve problems due to dependencies and conflicts.

    The first argument 'cache' is an instance of apt.Cache.
    """

    def __init__(self, cache):
        # type: (Cache) -> None
        self._resolver = apt_pkg.ProblemResolver(cache._depcache)
        self._cache = cache

    def clear(self, package):
        # type: (Package) -> None
        """Reset the package to the default state."""
        self._resolver.clear(package._pkg)

    def protect(self, package):
        # type: (Package) -> None
        """Protect a package so it won't be removed."""
        self._resolver.protect(package._pkg)

    def remove(self, package):
        # type: (Package) -> None
        """Mark a package for removal."""
        self._resolver.remove(package._pkg)

    def resolve(self):
        # type: () -> None
        """Resolve dependencies, try to remove packages where needed."""
        self._cache.cache_pre_change()
        self._resolver.resolve()
        self._cache.cache_post_change()

    def resolve_by_keep(self):
        # type: () -> None
        """Resolve dependencies, do not try to remove packages."""
        self._cache.cache_pre_change()
        self._resolver.resolve_by_keep()
        self._cache.cache_post_change()


# ----------------------------- experimental interface


class Filter(object):
    """ Filter base class """

    def apply(self, pkg):
        # type: (Package) -> bool
        """ Filter function, return True if the package matchs a
            filter criteria and False otherwise
        """
        return True


class MarkedChangesFilter(Filter):
    """ Filter that returns all marked changes """

    def apply(self, pkg):
        # type: (Package) -> bool
        if pkg.marked_install or pkg.marked_delete or pkg.marked_upgrade:
            return True
        else:
            return False


class InstalledFilter(Filter):
    """Filter that returns all installed packages.

    .. versionadded:: 1.0.0
    """

    def apply(self, pkg):
        # type: (Package) -> bool
        return pkg.is_installed


class _FilteredCacheHelper(object):
    """Helper class for FilteredCache to break a reference cycle."""

    def __init__(self, cache):
        # type: (Cache) -> None
        # Do not keep a reference to the cache, or you have a cycle!

        self._filtered = {}  # type: Dict[str,bool]
        self._filters = []  # type: List[Filter]
        cache.connect2("cache_post_change", self.filter_cache_post_change)
        cache.connect2("cache_post_open", self.filter_cache_post_change)

    def _reapply_filter(self, cache):
        # type: (Cache) -> None
        " internal helper to refilter "
        # Do not keep a reference to the cache, or you have a cycle!
        self._filtered = {}
        for pkg in cache:
            for f in self._filters:
                if f.apply(pkg):
                    self._filtered[pkg.name] = True
                    break

    def set_filter(self, filter):
        # type: (Filter) -> None
        """Set the current active filter."""
        self._filters = []
        self._filters.append(filter)

    def filter_cache_post_change(self, cache):
        # type: (Cache) -> None
        """Called internally if the cache changes, emit a signal then."""
        # Do not keep a reference to the cache, or you have a cycle!
        self._reapply_filter(cache)


class FilteredCache(object):
    """ A package cache that is filtered.

        Can work on a existing cache or create a new one
    """

    def __init__(self, cache=None, progress=None):
        # type: (Optional[Cache], Optional[OpProgress]) -> None
        if cache is None:
            self.cache = Cache(progress)
        else:
            self.cache = cache
        self._helper = _FilteredCacheHelper(self.cache)

    def __len__(self):
        # type: () -> int
        return len(self._helper._filtered)

    def __getitem__(self, key):
        # type: (str) -> Package
        return self.cache[key]

    def __iter__(self):
        # type: () -> Iterator[Package]
        for pkgname in self._helper._filtered:
            yield self.cache[pkgname]

    def keys(self):
        # type: () -> KeysView[str]
        return self._helper._filtered.keys()

    def has_key(self, key):
        # type: (object) -> bool
        return key in self

    def __contains__(self, key):
        # type: (object) -> bool
        try:
            # Normalize package name for multi arch
            return self.cache[key].name in self._helper._filtered
        except KeyError:
            return False

    def set_filter(self, filter):
        # type: (Filter) -> None
        """Set the current active filter."""
        self._helper.set_filter(filter)
        self.cache.cache_post_change()

    def filter_cache_post_change(self):
        # type: () -> None
        """Called internally if the cache changes, emit a signal then."""
        self._helper.filter_cache_post_change(self.cache)

    def __getattr__(self, key):
        # type: (str) -> Any
        """we try to look exactly like a real cache."""
        return getattr(self.cache, key)


def cache_pre_changed(cache):
    # type: (Cache) -> None
    print("cache pre changed")


def cache_post_changed(cache):
    # type: (Cache) -> None
    print("cache post changed")


def _test():
    # type: () -> None
    """Internal test code."""
    print("Cache self test")
    apt_pkg.init()
    cache = Cache(apt.progress.text.OpProgress())
    cache.connect2("cache_pre_change", cache_pre_changed)
    cache.connect2("cache_post_change", cache_post_changed)
    print(("aptitude" in cache))
    pkg = cache["aptitude"]
    print(pkg.name)
    print(len(cache))

    for pkgname in cache.keys():
        assert cache[pkgname].name == pkgname

    cache.upgrade()
    changes = cache.get_changes()
    print(len(changes))
    for pkg in changes:
        assert pkg.name

    # see if fetching works
    for dirname in ["/tmp/pytest", "/tmp/pytest/partial"]:
        if not os.path.exists(dirname):
            os.mkdir(dirname)
    apt_pkg.config.set("Dir::Cache::Archives", "/tmp/pytest")
    pm = apt_pkg.PackageManager(cache._depcache)
    fetcher = apt_pkg.Acquire(apt.progress.text.AcquireProgress())
    cache._fetch_archives(fetcher, pm, None)
    #sys.exit(1)

    print("Testing filtered cache (argument is old cache)")
    filtered = FilteredCache(cache)
    filtered.cache.connect2("cache_pre_change", cache_pre_changed)
    filtered.cache.connect2("cache_post_change", cache_post_changed)
    filtered.cache.upgrade()
    filtered.set_filter(MarkedChangesFilter())
    print(len(filtered))
    for pkgname in filtered.keys():
        assert pkgname == filtered[pkgname].name

    print(len(filtered))

    print("Testing filtered cache (no argument)")
    filtered = FilteredCache(progress=apt.progress.base.OpProgress())
    filtered.cache.connect2("cache_pre_change", cache_pre_changed)
    filtered.cache.connect2("cache_post_change", cache_post_changed)
    filtered.cache.upgrade()
    filtered.set_filter(MarkedChangesFilter())
    print(len(filtered))
    for pkgname in filtered.keys():
        assert pkgname == filtered[pkgname].name

    print(len(filtered))


if __name__ == '__main__':
    _test()

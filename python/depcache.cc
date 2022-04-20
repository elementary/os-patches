// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.cc,v 1.5 2003/06/03 03:03:23 mdz Exp $
/* ######################################################################

   DepCache - Wrapper for the depcache related functions

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "generic.h"
#include "apt_pkgmodule.h"

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/upgrade.h>
#include <Python.h>

#include <iostream>
#include "progress.h"

#ifndef _
#define _(x) (x)
#endif


#define VALIDATE_ITERATOR(I) do { \
   if ((I).Cache() != &depcache->GetCache()) { \
      PyErr_SetString(PyAptCacheMismatchError, "Object of different cache passed as argument to apt_pkg.DepCache method"); \
      return nullptr; \
   } \
} while(0)


// DepCache Class								/*{{{*/
// ---------------------------------------------------------------------



static PyObject *PkgDepCacheInit(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *pyCallbackInst = 0;
   if (PyArg_ParseTuple(Args, "|O", &pyCallbackInst) == 0)
      return 0;

   if(pyCallbackInst != 0) {
      PyOpProgress progress;
      progress.setCallbackInst(pyCallbackInst);
      depcache->Init(&progress);
   } else {
      depcache->Init(0);
   }

   pkgApplyStatus(*depcache);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *PkgDepCacheCommit(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *pyInstallProgressInst = 0;
   PyObject *pyFetchProgressInst = 0;
   if (PyArg_ParseTuple(Args, "OO",
			&pyFetchProgressInst, &pyInstallProgressInst) == 0) {
      return 0;
   }

   pkgAcquire Fetcher;
   if (Fetcher.GetLock(_config->FindDir("Dir::Cache::Archives")) == false) {
      Py_INCREF(Py_None);
      return HandleErrors(Py_None);
   }

   pkgRecords Recs(*depcache);
   if (_error->PendingError() == true)
      HandleErrors(Py_None);

   pkgSourceList List;
   if(!List.ReadMainList())
      return HandleErrors(Py_None);

   PyFetchProgress progress;
   progress.setCallbackInst(pyFetchProgressInst);

   pkgPackageManager *PM;
   PM = _system->CreatePM(depcache);

   Fetcher.SetLog(&progress);

   if(PM->GetArchives(&Fetcher, &List, &Recs) == false ||
      _error->PendingError() == true) {
      std::cerr << "Error in GetArchives" << std::endl;
      return HandleErrors();
   }

   //std::cout << "PM created" << std::endl;

   PyInstallProgress iprogress;
   iprogress.setCallbackInst(pyInstallProgressInst);

   // Run it
   while (1)
   {
      bool Transient = false;

      if (Fetcher.Run() == pkgAcquire::Failed)
	 return HandleErrors();

      // Print out errors
      bool Failed = false;
      for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
      {

	 //std::cout << "looking at: " << (*I)->DestFile
	 //	   << " status: " << (*I)->Status << std::endl;

	 if ((*I)->Status == pkgAcquire::Item::StatDone &&
	     (*I)->Complete == true)
	    continue;

	 if ((*I)->Status == pkgAcquire::Item::StatIdle)
	 {
	    //std::cout << "transient failure" << std::endl;

	    Transient = true;
	    //Failed = true;
	    continue;
	 }

	 //std::cout << "something is wrong!" << std::endl;

	 _error->Warning(_("Failed to fetch %s  %s\n"),(*I)->DescURI().c_str(),
			 (*I)->ErrorText.c_str());
	 Failed = true;
      }

      if (Transient == true && Failed == true)
      {
	 _error->Error(_("--fix-missing and media swapping is not currently supported"));
	 Py_INCREF(Py_None);
	 return HandleErrors(Py_None);
      }

      // Try to deal with missing package files
      if (Failed == true && PM->FixMissing() == false)
      {
	 //std::cerr << "Unable to correct missing packages." << std::endl;
	 _error->Error("Aborting install.");
	 Py_INCREF(Py_None);
	 return HandleErrors(Py_None);
      }

      // fail if something else went wrong
      //FIXME: make this more flexible, e.g. with a failedDl handler
      if(Failed)
	 Py_RETURN_FALSE;
      _system->UnLockInner(true);

      pkgPackageManager::OrderResult Res = iprogress.Run(PM);
      //std::cout << "iprogress.Run() returned: " << (int)Res << std::endl;

      if (Res == pkgPackageManager::Failed || _error->PendingError() == true) {
	 return HandleErrors(PyBool_FromLong(false));
      }
      if (Res == pkgPackageManager::Completed) {
	 //std::cout << "iprogress.Run() returned Completed " << std::endl;
     Py_RETURN_TRUE;
      }

      //std::cout << "looping again, install unfinished" << std::endl;

      // Reload the fetcher object and loop again for media swapping
      Fetcher.Shutdown();
      if (PM->GetArchives(&Fetcher,&List,&Recs) == false) {
	 Py_RETURN_FALSE;
      }
      _system->LockInner();
   }

   return HandleErrors(Py_None);
}

static PyObject *PkgDepCacheSetCandidateRelease(PyObject *Self,PyObject *Args)
{
   bool Success;
   PyObject *PackageObj;
   PyObject *VersionObj;
   const char *target_rel;
   std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > Changed;
   if (PyArg_ParseTuple(Args,"O!O!s",
			&PyPackage_Type, &PackageObj,
                        &PyVersion_Type, &VersionObj,
                        &target_rel) == 0)
      return 0;

   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);
   pkgCache::VerIterator &I = GetCpp<pkgCache::VerIterator>(VersionObj);
   if(I.end()) {
      return HandleErrors(PyBool_FromLong(false));
   }
   VALIDATE_ITERATOR(I);

   Success = depcache->SetCandidateRelease(I, target_rel, Changed);

   return HandleErrors(PyBool_FromLong(Success));
}

static PyObject *PkgDepCacheSetCandidateVer(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);
   PyObject *PackageObj;
   PyObject *VersionObj;
   if (PyArg_ParseTuple(Args,"O!O!",
			&PyPackage_Type, &PackageObj,
			&PyVersion_Type, &VersionObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);

   VALIDATE_ITERATOR(Pkg);

   pkgCache::VerIterator &I = GetCpp<pkgCache::VerIterator>(VersionObj);
   if(I.end()) {
      return HandleErrors(PyBool_FromLong(false));
   }
   VALIDATE_ITERATOR(I);

   if (I.ParentPkg() != Pkg) {
      PyErr_SetString(PyExc_ValueError, "Version does not belong to package");
      return nullptr;
   }

   depcache->SetCandidateVersion(I);

   return HandleErrors(PyBool_FromLong(true));
}

static PyObject *PkgDepCacheGetCandidateVer(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);
   PyObject *PackageObj;
   PyObject *CandidateObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);

   VALIDATE_ITERATOR(Pkg);

   pkgDepCache::StateCache & State = (*depcache)[Pkg];
   pkgCache::VerIterator I = State.CandidateVerIter(*depcache);

   if(I.end()) {
      Py_INCREF(Py_None);
      return Py_None;
   }
   CandidateObj = CppPyObject_NEW<pkgCache::VerIterator>(PackageObj,&PyVersion_Type,I);

   return CandidateObj;
}

static PyObject *PkgDepCacheUpgrade(PyObject *Self,PyObject *Args)
{
   bool res;
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   char distUpgrade=0;
   if (PyArg_ParseTuple(Args,"|b",&distUpgrade) == 0)
      return 0;

   Py_BEGIN_ALLOW_THREADS
   if(distUpgrade)
      res = APT::Upgrade::Upgrade(*depcache, 0);
   else
      res = APT::Upgrade::Upgrade(*depcache, APT::Upgrade::FORBID_REMOVE_PACKAGES |
                                             APT::Upgrade::FORBID_INSTALL_NEW_PACKAGES);
   Py_END_ALLOW_THREADS

   Py_INCREF(Py_None);
   return HandleErrors(PyBool_FromLong(res));
}

static PyObject *PkgDepCacheMinimizeUpgrade(PyObject *Self,PyObject *Args)
{
   bool res;
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   Py_BEGIN_ALLOW_THREADS
   res = pkgMinimizeUpgrade(*depcache);
   Py_END_ALLOW_THREADS

   Py_INCREF(Py_None);
   return HandleErrors(PyBool_FromLong(res));
}


static PyObject *PkgDepCacheReadPinFile(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);


   char *file=NULL;
   if (PyArg_ParseTuple(Args,"|s",&file) == 0)
      return 0;

   if(file == NULL)
      ReadPinFile((pkgPolicy&)depcache->GetPolicy());
   else
      ReadPinFile((pkgPolicy&)depcache->GetPolicy(), file);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}


static PyObject *PkgDepCacheFixBroken(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   bool res=true;
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   res &=pkgFixBroken(*depcache);
   res &=pkgMinimizeUpgrade(*depcache);

   return HandleErrors(PyBool_FromLong(res));
}


static PyObject *PkgDepCacheMarkKeep(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache*>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   depcache->MarkKeep(Pkg);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *PkgDepCacheSetReInstall(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache*>(Self);

   PyObject *PackageObj;
   char value = 0;
   if (PyArg_ParseTuple(Args,"O!b",&PyPackage_Type,&PackageObj, &value) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   depcache->SetReInstall(Pkg,value);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}


static PyObject *PkgDepCacheMarkDelete(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   char purge = 0;
   if (PyArg_ParseTuple(Args,"O!|b",&PyPackage_Type,&PackageObj, &purge) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   depcache->MarkDelete(Pkg,purge);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}


static PyObject *PkgDepCacheMarkInstall(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   char autoInst=1;
   char fromUser=1;
   if (PyArg_ParseTuple(Args,"O!|bb",&PyPackage_Type,&PackageObj,
			&autoInst, &fromUser) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);

   Py_BEGIN_ALLOW_THREADS
   depcache->MarkInstall(Pkg, autoInst, 0, fromUser);
   Py_END_ALLOW_THREADS

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *PkgDepCacheMarkAuto(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache*>(Self);

   PyObject *PackageObj;
   char value = 0;
   if (PyArg_ParseTuple(Args,"O!b",&PyPackage_Type,&PackageObj, &value) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   depcache->MarkAuto(Pkg,value);

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *PkgDepCacheIsUpgradable(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Upgradable()));
}

static PyObject *PkgDepCacheIsGarbage(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Garbage));
}

static PyObject *PkgDepCacheIsAutoInstalled(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Flags & pkgCache::Flag::Auto));
}

static PyObject *PkgDepCacheIsNowBroken(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.NowBroken()));
}

static PyObject *PkgDepCacheIsInstBroken(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.InstBroken()));
}


static PyObject *PkgDepCacheMarkedInstall(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.NewInstall()));
}


static PyObject *PkgDepCacheMarkedUpgrade(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Upgrade()));
}

static PyObject *PkgDepCacheMarkedDelete(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Delete()));
}

static PyObject *PkgDepCacheMarkedKeep(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Keep()));
}

static PyObject *PkgDepCacheMarkedDowngrade(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   return HandleErrors(PyBool_FromLong(state.Downgrade()));
}

static PyObject *PkgDepCacheMarkedReinstall(PyObject *Self,PyObject *Args)
{
   pkgDepCache *depcache = GetCpp<pkgDepCache *>(Self);

   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;

   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   pkgDepCache::StateCache &state = (*depcache)[Pkg];

   bool res = state.Install() && (state.iFlags & pkgDepCache::ReInstall);

   return HandleErrors(PyBool_FromLong(res));
}


static PyMethodDef PkgDepCacheMethods[] =
{
   {"init",PkgDepCacheInit,METH_VARARGS,
    "init(progress: apt.progress.base.OpProgress)\n\n"
    "Initialize the depcache (done automatically when constructing\n"
    "the object)."},
   {"get_candidate_ver",PkgDepCacheGetCandidateVer,METH_VARARGS,
    "get_candidate_ver(pkg: apt_pkg.Package) -> apt_pkg.Version\n\n"
    "Return the candidate version for the package, normally the version\n"
    "with the highest pin (changeable using set_candidate_ver)."},
   {"set_candidate_ver",PkgDepCacheSetCandidateVer,METH_VARARGS,
    "set_candidate_ver(pkg: apt_pkg.Package, ver: apt_pkg.Version) -> bool\n\n"
    "Set the candidate version of 'pkg' to 'ver'."},
   {"set_candidate_release",PkgDepCacheSetCandidateRelease,METH_VARARGS,
    "set_candidate_release(pkg: apt_pkg.Package, ver: apt_pkg.Version, rel: string) -> bool\n\n"
    "Sets not only the candidate version 'ver' for package 'pkg', "
    "but walks also down the dependency tree and checks if it is required "
    "to set the candidate of the dependency to a version from the given "
    "release string 'rel', too."},

   // global cache operations
   {"upgrade",PkgDepCacheUpgrade,METH_VARARGS,
    "upgrade([dist_upgrade: bool = True]) -> bool\n\n"
    "Mark the packages for upgrade under the same conditions apt-get\n"
    "upgrade does. If 'dist_upgrade' is True, also allow packages to\n"
    "be upgraded if they require installation/removal of other packages;\n"
    "just like apt-get dist-upgrade."},
   {"fix_broken",PkgDepCacheFixBroken,METH_VARARGS,
    "fix_broken() -> bool\n\n"
    "Fix broken packages."},
   {"read_pinfile",PkgDepCacheReadPinFile,METH_VARARGS,
    "read_pinfile([file: str])\n\n"
    "Read the pin policy"},
   {"minimize_upgrade",PkgDepCacheMinimizeUpgrade, METH_VARARGS,
    "minimize_upgrade() -> bool\n\n"
    "Go over the entire set of packages and try to keep each package\n"
    "marked for upgrade. If a conflict is generated then the package\n"
    "is restored."},
   // Manipulators
   {"mark_keep",PkgDepCacheMarkKeep,METH_VARARGS,
    "mark_keep(pkg: apt_pkg.Package)\n\n"
    "Mark package to be kept."},
   {"mark_delete",PkgDepCacheMarkDelete,METH_VARARGS,
    "mark_delete(pkg: apt_pkg.Package[, purge: bool = False])\n\n"
    "Mark package for deletion, and if 'purge' is True also for purging."},
   {"mark_install",PkgDepCacheMarkInstall,METH_VARARGS,
    "mark_install(pkg: apt_pkg.Package[, auto_inst=True, from_user=True])\n\n"
    "Mark the package for installation. The parameter 'auto_inst' controls\n"
    "whether the dependencies of the package are marked for installation\n"
    "as well. The parameter 'from_user' controls whether the package is\n"
    "registered as NOT automatically installed."},
   {"mark_auto",PkgDepCacheMarkAuto,METH_VARARGS,
    "mark_auto(pkg: apt_pkg.Package, auto: bool)\n\n"
    "Mark package as automatically installed (if auto=True),\n"
    "or as not automatically installed (if auto=False)."},
   {"set_reinstall",PkgDepCacheSetReInstall,METH_VARARGS,
    "set_reinstall(pkg: apt_pkg.Package, reinstall: bool)\n\n"
    "Set whether the package should be reinstalled (reinstall = True or False)."},
   // state information
   {"is_upgradable",PkgDepCacheIsUpgradable,METH_VARARGS,
    "is_upgradable(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is upgradable."},
   {"is_now_broken",PkgDepCacheIsNowBroken,METH_VARARGS,
    "is_now_broken(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is broken, taking marked changes into account."},
   {"is_inst_broken",PkgDepCacheIsInstBroken,METH_VARARGS,
    "is_inst_broken(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is broken, ignoring marked changes."},
   {"is_garbage",PkgDepCacheIsGarbage,METH_VARARGS,
    "is_garbage(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is garbage, i.e. whether it is automatically\n"
    "installed and the reverse dependencies are not installed anymore."},
   {"is_auto_installed",PkgDepCacheIsAutoInstalled,METH_VARARGS,
    "is_auto_installed(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked as automatically installed."},
   {"marked_install",PkgDepCacheMarkedInstall,METH_VARARGS,
    "marked_install(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked for installation."},
   {"marked_upgrade",PkgDepCacheMarkedUpgrade,METH_VARARGS,
    "marked_upgrade(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked for upgrade."},
   {"marked_delete",PkgDepCacheMarkedDelete,METH_VARARGS,
    "marked_delete(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked for removal."},
   {"marked_keep",PkgDepCacheMarkedKeep,METH_VARARGS,
    "marked_keep(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package should be kept."},
   {"marked_reinstall",PkgDepCacheMarkedReinstall,METH_VARARGS,
    "marked_reinstall(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked for re-installation."},
   {"marked_downgrade",PkgDepCacheMarkedDowngrade,METH_VARARGS,
    "marked_downgrade(pkg: apt_pkg.Package) -> bool\n\n"
    "Check whether the package is marked for downgrade."},
   // Action
   {"commit", PkgDepCacheCommit, METH_VARARGS,
    "commit(acquire_progress, install_progress)\n\n"
    "Commit all the marked changes. This method takes two arguments,\n"
    "'acquire_progress' takes an apt.progress.base.AcquireProgress\n"
    "object and 'install_progress' an apt.progress.base.InstallProgress\n"
    "object."},
   {}
};

#define depcache (GetCpp<pkgDepCache *>(Self))
static PyObject *PkgDepCacheGetKeepCount(PyObject *Self,void*) {
   return MkPyNumber(depcache->KeepCount());
}
static PyObject *PkgDepCacheGetInstCount(PyObject *Self,void*) {
   return MkPyNumber(depcache->InstCount());
}
static PyObject *PkgDepCacheGetDelCount(PyObject *Self,void*) {
   return MkPyNumber(depcache->DelCount());
}
static PyObject *PkgDepCacheGetBrokenCount(PyObject *Self,void*) {
   return MkPyNumber(depcache->BrokenCount());
}
static PyObject *PkgDepCacheGetUsrSize(PyObject *Self,void*) {
   return MkPyNumber(depcache->UsrSize());
}
static PyObject *PkgDepCacheGetDebSize(PyObject *Self,void*) {
   return MkPyNumber(depcache->DebSize());
}
#undef depcache

static PyObject *PkgDepCacheGetPolicy(PyObject *Self,void*) {
   PyObject *Owner = GetOwner<pkgDepCache*>(Self);
   pkgDepCache *DepCache = GetCpp<pkgDepCache*>(Self);
   pkgPolicy *Policy = (pkgPolicy *)&DepCache->GetPolicy();
   CppPyObject<pkgPolicy*> *PyPolicy =
        CppPyObject_NEW<pkgPolicy*>(Owner,&PyPolicy_Type,Policy);
   // Policy should not be deleted, it is managed by CacheFile.
   PyPolicy->NoDelete = true;
   return PyPolicy;
}


static PyGetSetDef PkgDepCacheGetSet[] = {
    {"broken_count",PkgDepCacheGetBrokenCount,0,
     "The number of packages with broken dependencies in the cache."},
    {"deb_size",PkgDepCacheGetDebSize,0,
     "The size of the packages which are needed for the changes to be\n"
     "applied."},
    {"del_count",PkgDepCacheGetDelCount,0,
     "The number of packages marked for removal."},
    {"inst_count",PkgDepCacheGetInstCount,0,
     "The number of packages marked for installation."},
    {"keep_count",PkgDepCacheGetKeepCount,0,
     "The number of packages marked for keep."},
    {"usr_size",PkgDepCacheGetUsrSize,0,
     "The amount of space required for installing/removing the packages,\n"
     "i.e. the Installed-Size of all packages marked for installation\n"
     "minus the Installed-Size of all packages for removal."},
    {"policy",PkgDepCacheGetPolicy,0,
     "The apt_pkg.Policy object used by this cache."},
    {}
};

static PyObject *PkgDepCacheNew(PyTypeObject *type,PyObject *Args,PyObject *kwds)
{
   PyObject *Owner;
   char *kwlist[] = {"cache", 0};
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"O!",kwlist,&PyCache_Type,
                                   &Owner) == 0)
      return 0;


   // the owner of the Python cache object is a cachefile object, get it
   PyObject *CacheFilePy = GetOwner<pkgCache*>(Owner);
   // get the pkgCacheFile from the cachefile
   pkgCacheFile *CacheF = GetCpp<pkgCacheFile*>(CacheFilePy);
   // and now the depcache
   pkgDepCache *depcache = (pkgDepCache *)(*CacheF);

   CppPyObject<pkgDepCache*> *DepCachePyObj;
   DepCachePyObj = CppPyObject_NEW<pkgDepCache*>(Owner,type,depcache);

   // Do not delete the underlying pointer, it is managed by the cachefile.
   DepCachePyObj->NoDelete = true;

   return HandleErrors(DepCachePyObj);
}

static char *doc_PkgDepCache = "DepCache(cache: apt_pkg.Cache)\n\n"
    "A DepCache() holds extra information on the state of the packages.\n\n"
    "The parameter 'cache' refers to an apt_pkg.Cache() object.";
PyTypeObject PyDepCache_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.DepCache",                  // tp_name
   sizeof(CppPyObject<pkgDepCache *>),   // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDeallocPtr<pkgDepCache *>,   // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   _PyAptObject_getattro,               // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC),
   doc_PkgDepCache,                     // tp_doc
   CppTraverse<pkgDepCache *>,     // tp_traverse
   CppClear<pkgDepCache *>,        // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   PkgDepCacheMethods,                  // tp_methods
   0,                                   // tp_members
   PkgDepCacheGetSet,                   // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgDepCacheNew,                      // tp_new
};




									/*}}}*/

#undef VALIDATE_ITERATOR
#define VALIDATE_ITERATOR(I) (void) 0     // FIXME: Need access to depcache of pkgProblemResolver

// pkgProblemResolver Class						/*{{{*/
// ---------------------------------------------------------------------
static PyObject *PkgProblemResolverNew(PyTypeObject *type,PyObject *Args,PyObject *kwds)
{
   PyObject *Owner;
   char *kwlist[] = {"depcache",0};
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"O!",kwlist,&PyDepCache_Type,
                                   &Owner) == 0)
      return 0;

   pkgDepCache *depcache = GetCpp<pkgDepCache*>(Owner);
   pkgProblemResolver *fixer = new pkgProblemResolver(depcache);
   CppPyObject<pkgProblemResolver*> *PkgProblemResolverPyObj;
   PkgProblemResolverPyObj = CppPyObject_NEW<pkgProblemResolver*>(Owner,
						      type,
						      fixer);
   HandleErrors(PkgProblemResolverPyObj);

   return PkgProblemResolverPyObj;
}


static PyObject *PkgProblemResolverResolve(PyObject *Self,PyObject *Args)
{
   bool res;
   pkgProblemResolver *fixer = GetCpp<pkgProblemResolver *>(Self);

   char brokenFix=1;
   if (PyArg_ParseTuple(Args,"|b",&brokenFix) == 0)
      return 0;

   Py_BEGIN_ALLOW_THREADS
   res = fixer->Resolve(brokenFix);
   Py_END_ALLOW_THREADS

   return HandleErrors(PyBool_FromLong(res));
}

static PyObject *PkgProblemResolverResolveByKeep(PyObject *Self,PyObject *Args)
{
   bool res;
   pkgProblemResolver *fixer = GetCpp<pkgProblemResolver *>(Self);
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   Py_BEGIN_ALLOW_THREADS
   res = fixer->ResolveByKeep();
   Py_END_ALLOW_THREADS

   return HandleErrors(PyBool_FromLong(res));
}

static PyObject *PkgProblemResolverProtect(PyObject *Self,PyObject *Args)
{
   pkgProblemResolver *fixer = GetCpp<pkgProblemResolver *>(Self);
   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;
   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   fixer->Protect(Pkg);
   Py_INCREF(Py_None);
   return HandleErrors(Py_None);

}
static PyObject *PkgProblemResolverRemove(PyObject *Self,PyObject *Args)
{
   pkgProblemResolver *fixer = GetCpp<pkgProblemResolver *>(Self);
   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;
   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   fixer->Remove(Pkg);
   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static PyObject *PkgProblemResolverClear(PyObject *Self,PyObject *Args)
{
   pkgProblemResolver *fixer = GetCpp<pkgProblemResolver *>(Self);
   PyObject *PackageObj;
   if (PyArg_ParseTuple(Args,"O!",&PyPackage_Type,&PackageObj) == 0)
      return 0;
   pkgCache::PkgIterator &Pkg = GetCpp<pkgCache::PkgIterator>(PackageObj);
   VALIDATE_ITERATOR(Pkg);
   fixer->Clear(Pkg);
   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}


static PyMethodDef PkgProblemResolverMethods[] =
{
   // config
   {"protect", PkgProblemResolverProtect, METH_VARARGS,
    "protect(pkg: apt_pkg.Package)\n\n"
    "Mark the package as protected in the resolver, meaning that its\n"
    "state will not be changed."},
   {"remove", PkgProblemResolverRemove, METH_VARARGS,
    "remove(pkg: apt_pkg.Package)\n\n"
    "Mark the package for removal in the resolver."},
   {"clear", PkgProblemResolverClear, METH_VARARGS,
    "clear(pkg: apt_pkg.Package)\n\n"
    "Revert the actions done by protect()/remove() on the package."},

   // Actions
   {"resolve", PkgProblemResolverResolve, METH_VARARGS,
    "resolve([fix_broken: bool = True]) -> bool\n\n"
    "Try to intelligently resolve problems by installing and removing\n"
    "packages. If 'fix_broken' is True, apt will try to repair broken\n"
    "dependencies of installed packages."},
   {"resolve_by_keep", PkgProblemResolverResolveByKeep, METH_VARARGS,
    "resolve_by_keep() -> bool\n\n"
    "Try to resolve problems only by using keep."},
   {}
};

static const char *problemresolver_doc =
    "ProblemResolver(depcache: apt_pkg.DepCache)\n\n"
    "ProblemResolver objects take care of resolving problems\n"
    "with dependencies. They mark packages for installation/\n"
    "removal and try to satisfy all dependencies.";
PyTypeObject PyProblemResolver_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.ProblemResolver",                       // tp_name
   sizeof(CppPyObject<pkgProblemResolver *>),   // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDeallocPtr<pkgProblemResolver *>,// tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,	                                // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   _PyAptObject_getattro,               // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC),
   problemresolver_doc,                 // tp_doc
   CppTraverse<pkgProblemResolver *>, // tp_traverse
   CppClear<pkgProblemResolver *>, // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   PkgProblemResolverMethods,           // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgProblemResolverNew,               // tp_new
};

									/*}}}*/

// pkgActionGroup Class						        /*{{{*/
// ---------------------------------------------------------------------

static const char *actiongroup_release_doc =
    "release()\n\n"
    "End the scope of this action group.  If this is the only action\n"
    "group bound to the cache, this will cause any deferred cleanup\n"
    "actions to be performed.";
static PyObject *PkgActionGroupRelease(PyObject *Self,PyObject *Args)
{
   pkgDepCache::ActionGroup *ag = GetCpp<pkgDepCache::ActionGroup*>(Self);
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;
   ag->release();
   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static const char *actiongroup__enter__doc =
    "__enter__() -> ActionGroup\n\n"
    "A dummy action which just returns the object itself, so it can\n"
    "be used as a context manager.";
static PyObject *PkgActionGroupEnter(PyObject *Self,PyObject *Args) {
   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;
   Py_INCREF(Self);
   return Self;
}

static const char *actiongroup__exit__doc =
    "__exit__(*excinfo) -> bool\n\n"
    "Same as release(), but for use as a context manager.";
static PyObject *PkgActionGroupExit(PyObject *Self,PyObject *Args) {
   pkgDepCache::ActionGroup *ag = GetCpp<pkgDepCache::ActionGroup*>(Self);
   if (ag != NULL)
      ag->release();
   Py_RETURN_FALSE;
}

static PyMethodDef PkgActionGroupMethods[] =
{
   {"release", PkgActionGroupRelease, METH_VARARGS, actiongroup_release_doc},
   {"__enter__", PkgActionGroupEnter, METH_VARARGS, actiongroup__enter__doc},
   {"__exit__", PkgActionGroupExit, METH_VARARGS, actiongroup__exit__doc},
   {}
};

static PyObject *PkgActionGroupNew(PyTypeObject *type,PyObject *Args,PyObject *kwds)
{
   PyObject *Owner;
   char *kwlist[] = {"depcache", 0};
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"O!",kwlist,&PyDepCache_Type,
                                   &Owner) == 0)
      return 0;

   pkgDepCache *depcache = GetCpp<pkgDepCache*>(Owner);
   pkgDepCache::ActionGroup *group = new pkgDepCache::ActionGroup(*depcache);
   CppPyObject<pkgDepCache::ActionGroup*> *PkgActionGroupPyObj;
   PkgActionGroupPyObj = CppPyObject_NEW<pkgDepCache::ActionGroup*>(Owner,
						      type,
						      group);
   HandleErrors(PkgActionGroupPyObj);

   return PkgActionGroupPyObj;

}

static char *doc_PkgActionGroup = "ActionGroup(depcache)\n\n"
    "Create a new ActionGroup() object. The parameter *depcache* refers to an\n"
    "apt_pkg.DepCache() object.\n\n"
    "ActionGroups disable certain cleanup actions, so modifying many packages\n"
    "is much faster.\n\n"
    "ActionGroup() can also be used with the 'with' statement, but be aware\n"
    "that the ActionGroup() is active as soon as it is created, and not just\n"
    "when entering the context. This means you can write::\n\n"
    "    with apt_pkg.ActionGroup(depcache):\n"
    "        depcache.markInstall(pkg)\n\n"
    "Once the block of the with statement is left, the action group is \n"
    "automatically released from the cache.";


PyTypeObject PyActionGroup_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.ActionGroup",               // tp_name
   sizeof(CppPyObject<pkgDepCache::ActionGroup*>),   // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDeallocPtr<pkgDepCache::ActionGroup*>,        // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   0,                                   // tp_as_sequence
   0,	                                // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   0,                                   // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC),
   doc_PkgActionGroup,                  // tp_doc
   CppTraverse<pkgDepCache::ActionGroup*>, // tp_traverse
   CppClear<pkgDepCache::ActionGroup*>, // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   PkgActionGroupMethods,               // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgActionGroupNew,                   // tp_new
};



									/*}}}*/

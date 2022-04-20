/*
 * policy.cc - Wrapper around pkgPolicy
 *
 * Copyright 2009 Julian Andres Klode <jak@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <Python.h>
#include "apt_pkgmodule.h"
#include "generic.h"
#include <apt-pkg/policy.h>

static PyObject *policy_new(PyTypeObject *type,PyObject *Args,
                                  PyObject *kwds) {
    PyObject *cache;
    char *kwlist[] = {"cache", NULL};
    if (PyArg_ParseTupleAndKeywords(Args, kwds, "O", kwlist, &cache) == 0)
        return 0;
    if (!PyObject_TypeCheck(cache, &PyCache_Type)) {
        PyErr_SetString(PyExc_TypeError,"`cache` must be a apt_pkg.Cache().");
        return 0;
    }
    pkgPolicy *policy = new pkgPolicy(GetCpp<pkgCache *>(cache));
    return CppPyObject_NEW<pkgPolicy*>(cache,&PyPolicy_Type,policy);
}

static char *policy_get_priority_doc =
    "get_priority(package: Union[apt_pkg.Package, apt_pkg.Version, apt_pkg.PackageFile]) -> int\n\n"
    "Return the priority of the package.";

PyObject *policy_get_priority(PyObject *self, PyObject *arg) {
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);
    if (PyObject_TypeCheck(arg, &PyVersion_Type)) {
        auto ver = GetCpp<pkgCache::VerIterator>(arg);
        return MkPyNumber(policy->GetPriority(ver));
    } else if (PyObject_TypeCheck(arg, &PyPackageFile_Type)) {
        pkgCache::PkgFileIterator pkgfile = GetCpp<pkgCache::PkgFileIterator>(arg);
        return MkPyNumber(policy->GetPriority(pkgfile));
    } else {
        PyErr_SetString(PyExc_TypeError,"Argument must be of Version or PackageFile.");
        return 0;
    }
}


static char *policy_set_priority_doc =
    "set_priority(which: Union[apt_pkg.Version, apt_pkg.PackageFile], priority: int) -> None\n\n"
    "Override priority for the given package/file. Behavior is undefined if"
    "a preferences file is read after that, or :meth:`init_defaults` is called.";
static PyObject *policy_set_priority(PyObject *self, PyObject *args) {
    PyObject *which;
    signed short priority;
    if (PyArg_ParseTuple(args, "Oh", &which, &priority) == 0)
        return 0;
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);

    if (PyObject_TypeCheck(which, &PyVersion_Type)) {
        auto ver = GetCpp<pkgCache::VerIterator>(which);
        policy->SetPriority(ver, priority);
    } else if (PyObject_TypeCheck(which, &PyPackageFile_Type)) {
        auto pkgfile = GetCpp<pkgCache::PkgFileIterator>(which);
        policy->SetPriority(pkgfile, priority);
    } else {
        PyErr_SetString(PyExc_TypeError,"Argument must be of Version or PackageFile.");
        return 0;
    }

    HandleErrors();
    Py_RETURN_NONE;
}

static char *policy_get_candidate_ver_doc =
    "get_match(package: apt_pkg.Package) -> Optional[apt_pkg.Version]\n\n"
    "Get the best package for the job.";

PyObject *policy_get_candidate_ver(PyObject *self, PyObject *arg) {
    if (PyObject_TypeCheck(arg, &PyPackage_Type)) {
        pkgPolicy *policy = GetCpp<pkgPolicy *>(self);
        pkgCache::PkgIterator pkg = GetCpp<pkgCache::PkgIterator>(arg);
        pkgCache::VerIterator ver = policy->GetCandidateVer(pkg);
        if (ver.end()) {
            HandleErrors();
            Py_RETURN_NONE;
        }
        return CppPyObject_NEW<pkgCache::VerIterator>(arg,&PyVersion_Type,
                                                           ver);
    } else {
        PyErr_SetString(PyExc_TypeError,"Argument must be of Package().");
        return 0;
    }
}

static char *policy_read_pinfile_doc =
    "read_pinfile(filename: str) -> bool\n\n"
    "Read the pin file given by filename (e.g. '/etc/apt/preferences')\n"
    "and add it to the policy.";

static PyObject *policy_read_pinfile(PyObject *self, PyObject *arg) {
    PyApt_Filename name;
    if (!name.init(arg))
        return 0;
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);

    return PyBool_FromLong(ReadPinFile(*policy, name));
}

#if (APT_PKG_MAJOR > 4 || (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 8))
static char *policy_read_pindir_doc =
    "read_pindir(dirname: str) -> bool\n\n"
    "Read the pin files in the given dir (e.g. '/etc/apt/preferences.d')\n"
    "and add them to the policy.";

static PyObject *policy_read_pindir(PyObject *self, PyObject *arg) {
    PyApt_Filename name;
    if (!name.init(arg))
        return 0;
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);

    return PyBool_FromLong(ReadPinDir(*policy, name));
}
#endif

static char *policy_create_pin_doc =
    "create_pin(type: str, pkg: str, data: str, priority: int)\n\n"
    "Create a pin for the policy. The parameter 'type' refers to one of the\n"
    "strings 'Version', 'Release', or 'Origin'. The argument 'pkg' is the\n"
    "name of the package. The parameter 'data' refers to the value\n"
    "(e.g. 'unstable' for type='Release') and the other possible options.\n"
    "The parameter 'priority' gives the priority of the pin.";

static PyObject *policy_create_pin(PyObject *self, PyObject *args) {
    pkgVersionMatch::MatchType match_type;
    const char *type, *pkg, *data;
    signed short priority;
    if (PyArg_ParseTuple(args, "sssh", &type, &pkg, &data, &priority) == 0)
        return 0;
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);
    if (strcmp(type,"Version") == 0 || strcmp(type, "version") == 0)
        match_type = pkgVersionMatch::Version;
    else if (strcmp(type,"Release") == 0 || strcmp(type, "release") == 0)
        match_type = pkgVersionMatch::Release;
    else if (strcmp(type,"Origin") == 0 || strcmp(type, "origin") == 0)
        match_type = pkgVersionMatch::Origin;
    else
        match_type = pkgVersionMatch::None;
    policy->CreatePin(match_type,pkg,data,priority);
    HandleErrors();
    Py_RETURN_NONE;
}

static char *policy_init_defaults_doc =
    "init_defaults()\n\n"
    "Initialize defaults. Needed after calling :meth:`create_pin()`\n"
    "with an empty `pkg` argument";
static PyObject *policy_init_defaults(PyObject *self, PyObject *args) {
    if (PyArg_ParseTuple(args, "") == 0)
        return 0;
    pkgPolicy *policy = GetCpp<pkgPolicy *>(self);
    policy->InitDefaults();
    HandleErrors();
    Py_RETURN_NONE;
}

static PyMethodDef policy_methods[] = {
    {"get_priority",(PyCFunction)policy_get_priority,METH_O,
     policy_get_priority_doc},
    {"set_priority",policy_set_priority,METH_VARARGS,policy_set_priority_doc},
    {"get_candidate_ver",(PyCFunction)policy_get_candidate_ver,METH_O,
     policy_get_candidate_ver_doc},
    {"read_pinfile",(PyCFunction)policy_read_pinfile,METH_O,
     policy_read_pinfile_doc},
#if (APT_PKG_MAJOR > 4 || (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 8))
    {"read_pindir",(PyCFunction)policy_read_pindir,METH_O,
     policy_read_pindir_doc},
#endif
    {"create_pin",policy_create_pin,METH_VARARGS,policy_create_pin_doc},
    {"init_defaults",policy_init_defaults,METH_VARARGS,policy_init_defaults_doc},
    {}
};

static char *policy_doc =
    "Policy(cache)\n\n"
    "Representation of the policy of the Cache object given by cache. This\n"
    "provides a superset of policy-related functionality compared to the\n"
    "DepCache class. The DepCache can be used for most purposes, but there\n"
    "may be some cases where a special policy class is needed.";

PyTypeObject PyPolicy_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "apt_pkg.Policy",                    // tp_name
    sizeof(CppPyObject<pkgPolicy*>),// tp_basicsize
    0,                                   // tp_itemsize
    // Methods
    CppDeallocPtr<pkgPolicy*>,      // tp_dealloc
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
    0,                                   // tp_getattro
    0,                                   // tp_setattro
    0,                                   // tp_as_buffer
    (Py_TPFLAGS_DEFAULT |                // tp_flags
     Py_TPFLAGS_BASETYPE |
     Py_TPFLAGS_HAVE_GC),
    policy_doc,                          // tp_doc
    CppTraverse<pkgPolicy*>,        // tp_traverse
    CppClear<pkgPolicy*>,           // tp_clear
    0,                                   // tp_richcompare
    0,                                   // tp_weaklistoffset
    0,                                   // tp_iter
    0,                                   // tp_iternext
    policy_methods,                      // tp_methods
    0,                                   // tp_members
    0,                                   // tp_getset
    0,                                   // tp_base
    0,                                   // tp_dict
    0,                                   // tp_descr_get
    0,                                   // tp_descr_set
    0,                                   // tp_dictoffset
    0,                                   // tp_init
    0,                                   // tp_alloc
    policy_new,                    // tp_new
};

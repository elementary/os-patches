/*
 * acquire-item.cc - Wrapper around pkgAcquire::Item and pkgAcqFile.
 *
 * Copyright 2004-2009 Canonical Ltd.
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

#include "generic.h"
#include "apt_pkgmodule.h"

#include <apt-pkg/acquire-item.h>
#include <map>

using namespace std;

inline pkgAcquire::Item *acquireitem_tocpp(PyObject *self)
{
    pkgAcquire::Item *itm = GetCpp<pkgAcquire::Item*>(self);
    if (itm == 0)
        PyErr_SetString(PyExc_ValueError, "Acquire() has been shut down or "
                        "the AcquireFile() object has been deallocated.");
    return itm;
}

static PyObject *acquireitem_get_complete(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? PyBool_FromLong(item->Complete) : 0;
}

static PyObject *acquireitem_get_desc_uri(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? CppPyString(item->DescURI()) : 0;
}

static PyObject *acquireitem_get_destfile(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? CppPyPath(item->DestFile) : 0;
}


static PyObject *acquireitem_get_error_text(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? CppPyString(item->ErrorText) : 0;
}

static PyObject *acquireitem_get_filesize(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? MkPyNumber(item->FileSize) : 0;
}

static PyObject *acquireitem_get_id(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? MkPyNumber(item->ID) : 0;
}

static PyObject *acquireitem_get_active_subprocess(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
#if APT_PKG_MAJOR >= 5
    return item ? Py_BuildValue("s", item->ActiveSubprocess.c_str()) : 0;
#else
    return item ? Py_BuildValue("s", item->Mode) : 0;
#endif
}

static PyObject *acquireitem_get_mode(PyObject *self, void *closure)
{
    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "AcquireItem.mode is deprecated, use AcquireItem.active_subprocess instead.", 1) == -1)
        return NULL;
    return acquireitem_get_active_subprocess(self, closure);
}

static PyObject *acquireitem_get_is_trusted(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? PyBool_FromLong(item->IsTrusted()) : 0;
}

static PyObject *acquireitem_get_local(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? PyBool_FromLong(item->Local) : 0;
}

static PyObject *acquireitem_get_partialsize(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? MkPyNumber(item->PartialSize) : 0;
}

static PyObject *acquireitem_get_status(PyObject *self, void *closure)
{
    pkgAcquire::Item *item = acquireitem_tocpp(self);
    return item ? MkPyNumber(item->Status) : 0;
}

static int acquireitem_set_id(PyObject *self, PyObject *value, void *closure)
{
    pkgAcquire::Item *Itm = acquireitem_tocpp(self);
    if (Itm == 0)
        return -1;
    if (PyLong_Check(value)) {
        Itm->ID = PyLong_AsUnsignedLong(value);
    }
    else if (PyInt_Check(value)) {
        Itm->ID = PyInt_AsLong(value);
    }
    else {
        PyErr_SetString(PyExc_TypeError, "value must be integer.");
        return -1;
    }
    return 0;
}


static PyGetSetDef acquireitem_getset[] = {
    {"complete",acquireitem_get_complete,0,
     "A boolean value determining whether the item has been fetched\n"
     "completely"},
    {"desc_uri",acquireitem_get_desc_uri,NULL,
     "A string describing the URI from which the item is acquired."},
    {"destfile",acquireitem_get_destfile,NULL,
     "The path to the file where the item will be stored."},
    {"error_text",acquireitem_get_error_text,NULL,
     "If an error occurred, a string describing the error; empty string\n"
     "otherwise."},
    {"filesize",acquireitem_get_filesize,NULL,
     "The size of the file (number of bytes). If unknown, it is set to 0."},
    {"id",acquireitem_get_id,acquireitem_set_id,
     "The ID of the item. An integer which can be set by progress classes."},
    {"mode",acquireitem_get_mode,NULL,
     "Old name for active_subprocess"},
    {"active_subprocess",acquireitem_get_active_subprocess,NULL,
     "The name of the active subprocess (for instance, 'gzip', 'rred' or 'gpgv')."},
    {"is_trusted",acquireitem_get_is_trusted,NULL,
     "Whether the item is trusted or not. Only True for packages\n"
     "which come from a repository signed with one of the keys in\n"
     "APT's keyring."},
    {"local",acquireitem_get_local,NULL,
     "Whether we are fetching a local item (copy:/) or not."},
    {"partialsize",acquireitem_get_partialsize,NULL,
     "The amount of data which has already been fetched (number of bytes)."},
    {"status",acquireitem_get_status,NULL,
     "An integer representing the item's status which can be compared\n"
     "against one of the STAT_* constants defined in this class."},
    {}
};

static PyObject *acquireitem_repr(PyObject *Self)
{
    pkgAcquire::Item *Itm = acquireitem_tocpp(Self);
    if (Itm == 0)
        return 0;

    string repr;
    strprintf(repr, "<%s object:"
              "Status: %i Complete: %i Local: %i IsTrusted: %i "
              "FileSize: %llu DestFile:'%s' "
              "DescURI: '%s' ID:%lu ErrorText: '%s'>",
              Self->ob_type->tp_name,
              Itm->Status, Itm->Complete, Itm->Local, Itm->IsTrusted(),
              Itm->FileSize, Itm->DestFile.c_str(),  Itm->DescURI().c_str(),
              Itm->ID,Itm->ErrorText.c_str());
    // Use CppPyPath here, the string may contain a path, so we should
    // decode it like one.
    return CppPyPath(repr);
}

static void acquireitem_dealloc(PyObject *self)
{
    CppDeallocPtr<pkgAcquire::Item*>(self);
}

static const char *acquireitem_doc =
    "Represent a single item to be fetched by an Acquire object.\n\n"
    "It is not possible to construct instances of this class directly.\n"
    "Prospective users should construct instances of a subclass such as\n"
    "AcquireFile instead. It is not possible to create subclasses on the\n"
    "Python level, only on the C++ level.";
PyTypeObject PyAcquireItem_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "apt_pkg.AcquireItem",         // tp_name
    sizeof(CppPyObject<pkgAcquire::Item*>),   // tp_basicsize
    0,                                   // tp_itemsize
    // Methods
    acquireitem_dealloc,                  // tp_dealloc
    0,                                   // tp_print
    0,                                   // tp_getattr
    0,                                   // tp_setattr
    0,                                   // tp_compare
    acquireitem_repr,                     // tp_repr
    0,                                   // tp_as_number
    0,                                   // tp_as_sequence
    0,                                   // tp_as_mapping
    0,                                   // tp_hash
    0,                                   // tp_call
    0,                                   // tp_str
    _PyAptObject_getattro,               // tp_getattro
    0,                                   // tp_setattro
    0,                                   // tp_as_buffer
    Py_TPFLAGS_DEFAULT |
    Py_TPFLAGS_HAVE_GC,                  // tp_flags
    acquireitem_doc,                // tp_doc
    CppTraverse<pkgAcquire::Item*>, // tp_traverse
    CppClear<pkgAcquire::Item*>,    // tp_clear
    0,                                   // tp_richcompare
    0,                                   // tp_weaklistoffset
    0,                                   // tp_iter
    0,                                   // tp_iternext
    0,                                   // tp_methods
    0,                                   // tp_members
    acquireitem_getset,                   // tp_getset
};

static PyObject *acquirefile_new(PyTypeObject *type, PyObject *Args, PyObject * kwds)
{
    PyObject *pyfetcher;
    const char *uri, *hash, *md5, *descr, *shortDescr;
    PyApt_Filename destDir, destFile;
    int size = 0;
    uri = hash = md5 = descr = shortDescr = destDir = destFile = "";

    // "md5" is only in this list for backward compatiblity, everyone should
    // use "hash"
    char *kwlist[] = {"owner", "uri", "hash", "size", "descr", "short_descr",
                      "destdir", "destfile", "md5", NULL
                     };
#if PY_MAJOR_VERSION >= 3
    const char *fmt = "O!s|sissO&O&$s";
#else
    // no "$" support to indicate that the remaining args are keyword only
    // in py2.x :/
    const char *fmt = "O!s|sissO&O&s";
#endif
    if (PyArg_ParseTupleAndKeywords(Args, kwds, fmt, kwlist,
                                    &PyAcquire_Type, &pyfetcher, &uri, &hash,
                                    &size, &descr, &shortDescr,
                                    PyApt_Filename::Converter, &destDir,
                                    PyApt_Filename::Converter, &destFile,
                                    &md5) == 0)
        return 0;
    // issue deprecation warning for md5
    if (strlen(md5) > 0) {
       PyErr_Warn(PyExc_DeprecationWarning,
                  "Using the md5 keyword is deprecated, please use 'hash' instead");
    }
    // support "md5" keyword for backward compatiblity
    if (strlen(hash) == 0 && strlen(md5) != 0)
       hash = md5;

    pkgAcquire *fetcher = GetCpp<pkgAcquire*>(pyfetcher);
    pkgAcqFile *af = new pkgAcqFile(fetcher,  // owner
                                    uri, // uri
                                    hash,  // hash
                                    size,   // size
                                    descr, // descr
                                    shortDescr,
                                    destDir,
                                    destFile); // short-desc
    CppPyObject<pkgAcqFile*> *AcqFileObj = CppPyObject_NEW<pkgAcqFile*>(pyfetcher, type);
    AcqFileObj->Object = af;
    return AcqFileObj;
}


static char *acquirefile_doc =
    "AcquireFile(owner, uri[, md5, size, descr, short_descr, destdir,"
    "destfile])\n\n"
    "Represent a file to be fetched. The parameter 'owner' points to\n"
    "an apt_pkg.Acquire object and the parameter 'uri' to the source\n"
    "location. Normally, the file will be stored in the current directory\n"
    "using the file name given in the URI. This directory can be changed\n"
    "by passing the name of a directory to the 'destdir' parameter. It is\n"
    "also possible to set a path to a file using the 'destfile' parameter,\n"
    "but both cannot be specified together.\n"
    "\n"
    "The parameters 'short_descr' and 'descr' can be used to specify\n"
    "a short description and a longer description for the item. This\n"
    "information is used by progress classes to refer to the item and\n"
    "should be short, for example, package name as 'short_descr' and\n"
    "and something like 'http://localhost sid/main python-apt 0.7.94.2'\n"
    "as 'descr'."
    "\n"
    "The parameters 'md5' and 'size' are used to verify the resulting\n"
    "file. The parameter 'size' is also to calculate the total amount\n"
    "of data to be fetched and is useful for resuming a interrupted\n"
    "download.\n\n"
    "All parameters can be given by name (i.e. as keyword arguments).";

PyTypeObject PyAcquireFile_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "apt_pkg.AcquireFile",                // tp_name
    sizeof(CppPyObject<pkgAcqFile*>),// tp_basicsize
    0,                                   // tp_itemsize
    // Methods
    acquireitem_dealloc,                 // tp_dealloc
    0,                                   // tp_print
    0,                                   // tp_getattr
    0,                                   // tp_setattr
    0,                                   // tp_compare
    0,                                   // tp_repr
    0,                                   // tp_as_number
    0,                                   // tp_as_sequence
    0,	                                 // tp_as_mapping
    0,                                   // tp_hash
    0,                                   // tp_call
    0,                                   // tp_str
    0,                                   // tp_getattro
    0,                                   // tp_setattro
    0,                                   // tp_as_buffer
    Py_TPFLAGS_DEFAULT |                 // tp_flags
    Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_HAVE_GC,
    acquirefile_doc,                     // tp_doc
    CppTraverse<pkgAcqFile*>,       // tp_traverse
    CppClear<pkgAcqFile*>,          // tp_clear
    0,                                   // tp_richcompare
    0,                                   // tp_weaklistoffset
    0,                                   // tp_iter
    0,                                   // tp_iternext
    0,                                   // tp_methods
    0,                                   // tp_members
    0,                                   // tp_getset
    &PyAcquireItem_Type,                 // tp_base
    0,                                   // tp_dict
    0,                                   // tp_descr_get
    0,                                   // tp_descr_set
    0,                                   // tp_dictoffset
    0,                                   // tp_init
    0,                                   // tp_alloc
    acquirefile_new,                     // tp_new
};


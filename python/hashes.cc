/* hashes.cc - Wrapper around apt-pkg's Hashes.
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
#include "generic.h"
#include "apt_pkgmodule.h"
#include <apt-pkg/hashes.h>

static PyObject *hashes_new(PyTypeObject *type,PyObject *args,
                            PyObject *kwds)
{
    return CppPyObject_NEW<Hashes>(NULL, type);
}

static int hashes_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *object = 0;
    int Fd;
    char *kwlist[] = {"object",  NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|O:__init__", kwlist,
                                    &object) == 0)
        return -1;
    if (object == 0)
        return 0;
    Hashes &hashes = GetCpp<Hashes>(self);

    if (PyBytes_Check(object) != 0) {
        char *s;
        Py_ssize_t len;
        PyBytes_AsStringAndSize(object, &s, &len);
        Py_BEGIN_ALLOW_THREADS
        hashes.Add((const unsigned char*)s, len);
        Py_END_ALLOW_THREADS
    }
    else if ((Fd = PyObject_AsFileDescriptor(object)) != -1) {
        struct stat St;
        bool err = false;
        Py_BEGIN_ALLOW_THREADS
        err = fstat(Fd, &St) != 0 || hashes.AddFD(Fd, St.st_size) == false;
        Py_END_ALLOW_THREADS
        if (err) {
            PyErr_SetFromErrno(PyAptError);
            return -1;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError,
                        "__init__() only understand bytes and files");
        return -1;
    }
    return 0;
}

static PyObject *hashes_get_hashes(PyObject *self, void*)
{
    auto py = CppPyObject_NEW<HashStringList>(nullptr, &PyHashStringList_Type);

    py->Object = GetCpp<Hashes>(self).GetHashStringList();
    return py;
}


static PyGetSetDef hashes_getset[] = {
    {"hashes",hashes_get_hashes,0,
     "A :class:`HashStringList` of all hashes.\n\n"
     ".. versionadded:: 1.1"},
    {}
};

static char *hashes_doc =
    "Hashes([object: (bytes, file)])\n\n"
    "Calculate hashes for the given object. It can be used to create all\n"
    "supported hashes for a file.\n\n"
    "The parameter *object* can be a bytestring, an object providing the\n"
    "fileno() method, or an integer describing a file descriptor.";

PyTypeObject PyHashes_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "apt_pkg.Hashes",                  // tp_name
    sizeof(CppPyObject<Hashes>),       // tp_basicsize
    0,                                 // tp_itemsize
    // Methods
    CppDealloc<Hashes>,                // tp_dealloc
    0,                                 // tp_print
    0,                                 // tp_getattr
    0,                                 // tp_setattr
    0,                                 // tp_compare
    0,                                 // tp_repr
    0,                                 // tp_as_number
    0,                                 // tp_as_sequence
    0,                                 // tp_as_mapping
    0,                                 // tp_hash
    0,                                 // tp_call
    0,                                 // tp_str
    0,                                 // tp_getattro
    0,                                 // tp_setattro
    0,                                 // tp_as_buffer
    Py_TPFLAGS_DEFAULT |               // tp_flags
    Py_TPFLAGS_BASETYPE,
    hashes_doc,                        // tp_doc
    0,                                 // tp_traverse
    0,                                 // tp_clear
    0,                                 // tp_richcompare
    0,                                 // tp_weaklistoffset
    0,                                 // tp_iter
    0,                                 // tp_iternext
    0,                                 // tp_methods
    0,                                 // tp_members
    hashes_getset,                     // tp_getset
    0,                                 // tp_base
    0,                                 // tp_dict
    0,                                 // tp_descr_get
    0,                                 // tp_descr_set
    0,                                 // tp_dictoffset
    hashes_init,                       // tp_init
    0,                                 // tp_alloc
    hashes_new,                        // tp_new
};

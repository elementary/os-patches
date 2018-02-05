/* hashstringlist.cc - Wrapper around apt-pkg's Hashes.
 *
 * Copyright 2015 Julian Andres Klode <jak@debian.org>
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

static PyObject *hashstringlist_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    return CppPyObject_NEW<HashStringList> (nullptr, type);
}

static int hashstringlist_init(PyObject *self, PyObject *args,
                               PyObject *kwds)
{
    char *kwlist[] = { NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist) == 0)
        return -1;

    return 0;
}


static const char hashstringlist_find_doc[] =
    "find(type: str = \"\") -> HashString\n\n"
    "Find a hash of the given type, or the best one, if the argument\n"
    "is empty or not specified.";
static PyObject *hashstringlist_find(PyObject *self, PyObject *args)
{
    char *type = "";

    if (PyArg_ParseTuple(args, "|s", &type) == 0)
        return 0;

    HashString *hs = new HashString;
    *hs = *(GetCpp<HashStringList>(self).find(type));

    return HandleErrors(PyHashString_FromCpp(hs, true, nullptr));
}

static const char hashstringlist_append_doc[] =
    "append(object: HashString)\n\n"
    "Append the given HashString to this list.";
static PyObject *hashstringlist_append(PyObject *self, PyObject *args)
{
    PyObject *o;

    if (PyArg_ParseTuple(args, "O!", &PyHashString_Type, &o) == 0)
        return 0;

    GetCpp<HashStringList>(self).push_back(*PyHashString_ToCpp(o));
    Py_RETURN_NONE;
}

static const char hashstringlist_verify_file_doc[] =
    "verify_file(filename: str) -> bool\n\n"
    "Verify that the file with the given name matches all hashes in\n"
    "the list.";
static PyObject *hashstringlist_verify_file(PyObject *self, PyObject *args)
{
    PyApt_Filename filename;

    if (PyArg_ParseTuple(args, "O&", PyApt_Filename::Converter, &filename) == 0)
        return 0;

    bool res = GetCpp<HashStringList>(self).VerifyFile(filename);

    PyObject *PyRes = PyBool_FromLong(res);
    return HandleErrors(PyRes);
}

static PyObject *hashstringlist_get_file_size(PyObject *self, void*) {
    return MkPyNumber(GetCpp<HashStringList>(self).FileSize());
}

static int hashstringlist_set_file_size(PyObject *self, PyObject *value, void *) {
    if (PyLong_Check(value)) {
        if (PyLong_AsUnsignedLongLong(value) == (unsigned long long) -1) {
            return 1;
        }
        GetCpp<HashStringList>(self).FileSize(PyLong_AsUnsignedLongLong(value));
    } else if (PyInt_Check(value)) {
        if (PyInt_AsLong(value) < 0) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_OverflowError,
                                "The file_size value must be positive");
                return 1;
        }
        GetCpp<HashStringList>(self).FileSize(PyInt_AsLong(value));
    } else {
        PyErr_SetString(PyExc_TypeError,
                        "The file_size value must be an integer or long");
        return 1;
    }

    return 0;
}

/* The same for groups */
static Py_ssize_t hashstringlist_len(PyObject *self)
{
    return GetCpp <HashStringList>(self).size();
}

static PyObject *hashstringlist_getitem(PyObject *iSelf, Py_ssize_t index)
{
    HashStringList &self = GetCpp<HashStringList>(iSelf);

    if (index < 0 || (size_t) index >= self.size())
        return PyErr_Format(PyExc_IndexError, "Out of range: %zd", index);

    /* Copy over, safer than using a reference to the vector element */
    HashString *hs = new HashString;
    (*hs) = *(self.begin() + index);

    return PyHashString_FromCpp(hs, true, nullptr);
}

static PySequenceMethods hashstringlist_seq_methods = {
    hashstringlist_len,
    0,                          // concat
    0,                          // repeat
    hashstringlist_getitem,
    0,                          // slice
    0,                          // assign item
    0                           // assign slice
};

static PyMethodDef hashstringlist_methods[] =
{
    {"verify_file",hashstringlist_verify_file,METH_VARARGS,
     hashstringlist_verify_file_doc},
    {"find",hashstringlist_find,METH_VARARGS,
     hashstringlist_find_doc},
    {"append",hashstringlist_append,METH_VARARGS,
     hashstringlist_append_doc},
    {}
};

static PyGetSetDef hashstringlist_getset[] = {
    {"file_size",hashstringlist_get_file_size,hashstringlist_set_file_size,
     "If a file size is part of the list, return it, otherwise 0."},
    {}
};


static char *hashstringlist_doc =
    "HashStringList()\n\n"
    "Manage a list of HashStrings.\n\n"
    "The list knows which hash is the best and provides convenience\n"
    "methods for file verification.\n\n"
    ".. versionadded:: 1.1";

PyTypeObject PyHashStringList_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
        "apt_pkg.HashStringList",       // tp_name
    sizeof(CppPyObject<HashStringList>),     // tp_basicsize
    0,                          // tp_itemsize
    // Methods
    CppDealloc<HashStringList>, // tp_dealloc
    0,                          // tp_print
    0,                          // tp_getattr
    0,                          // tp_setattr
    0,                          // tp_compare
    0,                          // tp_repr
    0,                          // tp_as_number
    &hashstringlist_seq_methods,        // tp_as_sequence
    0,                          // tp_as_mapping
    0,                          // tp_hash
    0,                          // tp_call
    0,                          // tp_str
    0,                          // tp_getattro
    0,                          // tp_setattro
    0,                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT,         // tp_flags
    hashstringlist_doc,         // tp_doc
    0,                          // tp_traverse
    0,                          // tp_clear
    0,                          // tp_richcompare
    0,                          // tp_weaklistoffset
    0,                          // tp_iter
    0,                          // tp_iternext
    hashstringlist_methods,     // tp_methods
    0,                          // tp_members
    hashstringlist_getset,      // tp_getset
    0,                          // tp_base
    0,                          // tp_dict
    0,                          // tp_descr_get
    0,                          // tp_descr_set
    0,                          // tp_dictoffset
    hashstringlist_init,        // tp_init
    0,                          // tp_alloc
    hashstringlist_new,         // tp_new
};

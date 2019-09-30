/*
 * Copyright © 2013 marmuta
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "osk_module.h"

#include <hunspell/hunspell.h>

typedef struct {
    PyObject_HEAD
    Hunhandle* hh;
} OskHunspell;

OSK_REGISTER_TYPE (OskHunspell, osk_hunspell, "Hunspell")

static int
osk_hunspell_init (OskHunspell *oh, PyObject *args, PyObject *kwds)
{
    char* aff_path;
    char* dic_path;

    if (!PyArg_ParseTuple (args, "zs:__init__", &aff_path, &dic_path))
        return -1;

    if (!aff_path)
        aff_path = "";

    oh->hh = Hunspell_create(aff_path, dic_path);
    if (oh->hh == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "failed to create hunspell handle");
        return -1;
    }
    return 0;
}

static void
osk_hunspell_dealloc (OskHunspell *oh)
{
    if (oh->hh)
        Hunspell_destroy(oh->hh);

    OSK_FINISH_DEALLOC (oh);
}

static PyObject *
osk_hunspell_spell (PyObject *self, PyObject *args)
{
    OskHunspell *oh = (OskHunspell*) self;
    char* word; // in dictionary encoding

    char* encoding = Hunspell_get_dic_encoding(oh->hh);
    if (!encoding)
    {
        PyErr_SetString(PyExc_MemoryError, "unknown dictionary encoding");
        return NULL;
    }

    if (!PyArg_ParseTuple (args, "es:spell", encoding, &word))
        return NULL;

    int res = Hunspell_spell(oh->hh, word);

    return PyLong_FromLong(res);
}

static PyObject *
osk_hunspell_suggest (PyObject *self, PyObject *args)
{
    OskHunspell *oh = (OskHunspell*) self;
    PyObject* result = NULL;
    char* word; // in dictionary encoding

    char* encoding = Hunspell_get_dic_encoding(oh->hh);
    if (!encoding)
    {
        PyErr_SetString(PyExc_MemoryError, "unknown dictionary encoding");
        return NULL;
    }

    if (!PyArg_ParseTuple (args, "es:suggest", encoding, &word))
        return NULL;

    char** slst;
    int n = Hunspell_suggest(oh->hh, &slst, word);

    result = PyTuple_New(n);
    if (!result)
    {
        PyErr_SetString(PyExc_MemoryError, "failed to allocate result tuple");
        return NULL;
    }

    int i;
    for (i = 0; i < n; i++)
    {
        PyObject* suggestion = PyUnicode_Decode(slst[i], strlen(slst[i]),
                                                encoding, NULL);
        if (!suggestion)
        {
            PyErr_SetString(PyExc_MemoryError, "failed to decode suggestion");
            Py_DECREF(result);
            return NULL;
        }

        PyTuple_SetItem(result, i, suggestion);
    }

    Hunspell_free_list(oh->hh, &slst, n);

    return result;
}

static PyObject *
osk_hunspell_get_encoding (PyObject *self, PyObject *args)
{
    OskHunspell *oh = (OskHunspell*) self;
    char* encoding = Hunspell_get_dic_encoding(oh->hh);
    if (encoding)
        return PyUnicode_FromString(encoding);
    Py_RETURN_NONE;
}

static PyMethodDef osk_hunspell_methods[] = {
    { "spell",
        osk_hunspell_spell,
        METH_VARARGS, NULL },
    { "suggest",
        osk_hunspell_suggest,
        METH_VARARGS, NULL },
    { "get_encoding",
        osk_hunspell_get_encoding,
        METH_VARARGS, NULL },
    { NULL, NULL, 0, NULL }
};


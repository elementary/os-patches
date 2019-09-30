/*
 * Copyright © 2011 Gerd Kohlberger
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

#ifndef __OSK_MODULE__
#define __OSK_MODULE__

#include <Python.h>
#include <structmember.h>

/**
 * Python2 to Python3 conversion
**/
#if PY_MAJOR_VERSION >= 3
    #define PyString_FromString PyUnicode_FromString
    #define PyString_FromStringAndSize PyUnicode_FromStringAndSize
    #define PyString_AsString PyBytes_AsString

    #define PyInt_FromLong PyLong_FromLong
    #define PyInt_AsLong PyLong_AsLong
    #define PyInt_Check PyLong_Check
#else
#endif

/**
 * Get the module exception object.
 */
#define OSK_EXCEPTION (__osk_error)

#define OSK_DEFINE_TYPE(__TypeName, __type_name, __PyName) \
static PyTypeObject __type_name##_type = { \
    PyVarObject_HEAD_INIT(&PyType_Type, 0) \
    "osk." __PyName,                          /* tp_name */ \
    sizeof (__TypeName),                      /* tp_basicsize */ \
    0,                                        /* tp_itemsize */ \
    (destructor) __type_name##_dealloc,       /* tp_dealloc */ \
    0,                                        /* tp_print */ \
    0,                                        /* tp_getattr */ \
    0,                                        /* tp_setattr */ \
    0,                                        /* tp_compare */ \
    0,                                        /* tp_repr */ \
    0,                                        /* tp_as_number */ \
    0,                                        /* tp_as_sequence */ \
    0,                                        /* tp_as_mapping */ \
    0,                                        /* tp_hash */ \
    0,                                        /* tp_call */ \
    0,                                        /* tp_str */ \
    0,                                        /* tp_getattro */ \
    0,                                        /* tp_setattro */ \
    0,                                        /* tp_as_buffer */ \
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */ \
    __PyName " objects",                      /* tp_doc */ \
    0,                                        /* tp_traverse */ \
    0,                                        /* tp_clear */ \
    0,                                        /* tp_richcompare */ \
    0,                                        /* tp_weaklistoffset */ \
    0,                                        /* tp_iter */ \
    0,                                        /* tp_iternext */ \
    __type_name##_methods,                    /* tp_methods */ \
    0,                                        /* tp_members */ \
    0,                                        /* tp_getset */ \
    0,                                        /* tp_base */ \
    0,                                        /* tp_dict */ \
    0,                                        /* tp_descr_get */ \
    0,                                        /* tp_descr_set */ \
    0,                                        /* tp_dictoffset */ \
    (initproc) __type_name##_init,            /* tp_init */ \
    0,                                        /* tp_alloc */ \
    PyType_GenericNew,                        /* tp_new */ \
};

#define OSK_DEFINE_TYPE_WITH_MEMBERS(__TypeName, __type_name, __PyName) \
static PyMemberDef __type_name##_members[]; \
static PyGetSetDef __type_name##_getsetters[]; \
\
static PyTypeObject __type_name##_type = { \
    PyVarObject_HEAD_INIT(&PyType_Type, 0) \
    "osk." __PyName,                          /* tp_name */ \
    sizeof (__TypeName),                      /* tp_basicsize */ \
    0,                                        /* tp_itemsize */ \
    (destructor) __type_name##_dealloc,       /* tp_dealloc */ \
    0,                                        /* tp_print */ \
    0,                                        /* tp_getattr */ \
    0,                                        /* tp_setattr */ \
    0,                                        /* tp_compare */ \
    0,                                        /* tp_repr */ \
    0,                                        /* tp_as_number */ \
    0,                                        /* tp_as_sequence */ \
    0,                                        /* tp_as_mapping */ \
    0,                                        /* tp_hash */ \
    0,                                        /* tp_call */ \
    0,                                        /* tp_str */ \
    0,                                        /* tp_getattro */ \
    0,                                        /* tp_setattro */ \
    0,                                        /* tp_as_buffer */ \
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */ \
    __PyName " objects",                      /* tp_doc */ \
    0,                                        /* tp_traverse */ \
    0,                                        /* tp_clear */ \
    0,                                        /* tp_richcompare */ \
    0,                                        /* tp_weaklistoffset */ \
    0,                                        /* tp_iter */ \
    0,                                        /* tp_iternext */ \
    __type_name##_methods,                    /* tp_methods */ \
    __type_name##_members,                    /* tp_members */ \
    __type_name##_getsetters,                 /* tp_getset */ \
    0,                                        /* tp_base */ \
    0,                                        /* tp_dict */ \
    0,                                        /* tp_descr_get */ \
    0,                                        /* tp_descr_set */ \
    0,                                        /* tp_dictoffset */ \
    (initproc) __type_name##_init,            /* tp_init */ \
    0,                                        /* tp_alloc */ \
    PyType_GenericNew,                        /* tp_new */ \
};

#define OSK_REGISTER_TYPE_BEGIN(__TypeName, __type_name) \
static int __type_name##_init (__TypeName *self, PyObject *args, PyObject *kwds); \
static void __type_name##_dealloc (__TypeName *self); \
static PyMethodDef __type_name##_methods[];

#define OSK_REGISTER_TYPE_END(__type_name, __PyName) \
void __##__type_name##_register_type (PyObject *module) \
{ \
    if (PyType_Ready (&__type_name##_type) < 0) \
        Py_FatalError ("osk: Cannot initialize " __PyName " type."); \
    Py_INCREF (&__type_name##_type); \
    if (PyModule_AddObject (module, __PyName, (PyObject *) &__type_name##_type) < 0) \
        Py_FatalError ("osk: Cannot add " __PyName " object."); \
}

/**
 * Register a new python type.
 */
#define OSK_REGISTER_TYPE(__TypeName, __type_name, __PyName) \
    OSK_REGISTER_TYPE_BEGIN(__TypeName, __type_name) \
    OSK_DEFINE_TYPE(__TypeName, __type_name, __PyName) \
    OSK_REGISTER_TYPE_END(__type_name, __PyName)

/**
 * Register a new python type with member definitions.
 */
#define OSK_REGISTER_TYPE_WITH_MEMBERS(__TypeName, __type_name, __PyName) \
    OSK_REGISTER_TYPE_BEGIN(__TypeName, __type_name) \
    OSK_DEFINE_TYPE_WITH_MEMBERS(__TypeName, __type_name, __PyName) \
    OSK_REGISTER_TYPE_END(__type_name, __PyName)

/**
 * Sugar for the dealloc vfunc of Python objects.
 */
#define OSK_FINISH_DEALLOC(o) (Py_TYPE(o)->tp_free ((PyObject *) (o)))

extern PyObject *__osk_error;

void    __osk_virtkey_register_type      (PyObject *module);
void    __osk_audio_register_type        (PyObject *module);
void    __osk_dconf_register_type        (PyObject *module);
void    __osk_devices_register_type      (PyObject *module);
void    __osk_device_event_register_type (PyObject *module);
void    __osk_hunspell_register_type     (PyObject *module);
void    __osk_struts_register_type       (PyObject *module);
void    __osk_click_mapper_register_type (PyObject *module);
void    __osk_util_register_type         (PyObject *module);

void osk_util_idle_call (PyObject *callback, PyObject *arglist);

#endif /* __OSK_MODULE__ */

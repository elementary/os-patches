/* ######################################################################

   PkgManagerProgress - Wrapper for pkgPackageManagerProgress

   ##################################################################### */

#include "generic.h"
#include "apt_pkgmodule.h"

#include <apt-pkg/install-progress.h>

#include <iostream>

static PyObject *PkgManagerProgressFancyNew(PyTypeObject *type,PyObject *Args,PyObject *kwds)
{
   char *kwlist[] = {0};
   if (PyArg_ParseTupleAndKeywords(Args,kwds,"",kwlist) == 0)
      return 0;
   return CppPyObject_NEW<APT::Progress::PackageManagerFancy*>(NULL, type, new APT::Progress::PackageManagerFancy());
}

static const char *packagemanagerprogressfancy_doc =
   "_PackageManagerProgressFancy will show nice text install progress";
PyTypeObject PyPackageManagerProgressFancy_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.PackageManagerProgressFancy",     // tp_name
   sizeof(CppPyObject<APT::Progress::PackageManagerFancy*>), // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDeallocPtr<APT::Progress::PackageManagerFancy*>,   // tp_dealloc
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
   Py_TPFLAGS_DEFAULT,                  // tp_flag,
   packagemanagerprogressfancy_doc,     // tp_doc
   0,                                   // tp_traverse
   0,                                   // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   0,                                   // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgManagerProgressFancyNew,          // tp_new
};



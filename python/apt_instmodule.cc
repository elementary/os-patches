// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt_instmodule.cc,v 1.3 2002/01/08 06:53:04 jgg Exp $
/* ######################################################################

   apt_intmodule - Top level for the python module. Create the internal
                   structures for the module in the interpriter.

   Note, this module shares state (particularly global config) with the
   apt_pkg module.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "apt_instmodule.h"
#include "generic.h"

#include <apt-pkg/debfile.h>
#include <apt-pkg/error.h>

#include <sys/stat.h>
#include <unistd.h>
#include <Python.h>
									/*}}}*/

PyObject *PyAptError;
static PyMethodDef *methods = 0;


static const char *apt_inst_doc =
    "Functions for working with ar/tar archives and .deb packages.\n\n"
    "This module provides useful classes and functions to work with\n"
    "archives, modelled after the 'TarFile' class in the 'tarfile' module.";
#define ADDTYPE(mod,name,type) { \
    if (PyType_Ready(type) == -1) RETURN(0); \
    Py_INCREF(type); \
    PyModule_AddObject(mod,name,(PyObject *)type); }


#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "apt_inst",
        apt_inst_doc,
        -1,
        methods,
        0,
        0,
        0,
        0
};
#define RETURN(x) return x
#define INIT_ERROR return 0
extern "C" PyObject * PyInit_apt_inst()
#else
#define INIT_ERROR return
extern "C" void initapt_inst()
#define RETURN(x) return
#endif
{
#if PY_MAJOR_VERSION >= 3
   PyObject *module = PyModule_Create(&moduledef);
#else
   PyObject *module = Py_InitModule3("apt_inst",methods, apt_inst_doc);
#endif

   PyObject *apt_pkg = PyImport_ImportModule("apt_pkg");
   if (apt_pkg == NULL)
      INIT_ERROR;
   PyAptError = PyObject_GetAttrString(apt_pkg, "Error");
   if (PyAptError == NULL)
      INIT_ERROR;

   PyModule_AddObject(module,"Error",PyAptError);
   ADDTYPE(module,"ArMember",&PyArMember_Type);
   ADDTYPE(module,"ArArchive",&PyArArchive_Type);
   ADDTYPE(module,"DebFile",&PyDebFile_Type);
   ADDTYPE(module,"TarFile",&PyTarFile_Type);
   ADDTYPE(module,"TarMember",&PyTarMember_Type);
   RETURN(module);
}

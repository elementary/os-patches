// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgsrcrecords.cc,v 1.2 2003/12/26 17:04:22 mdz Exp $
/* ######################################################################

   Package Records - Wrapper for the package records functions

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "generic.h"
#include "apt_pkgmodule.h"

#include <apt-pkg/sourcelist.h>

#include <Python.h>
									/*}}}*/

// PkgSrcRecordFiles Class						/*{{{*/
// ---------------------------------------------------------------------
typedef pkgSrcRecords::File PkgSrcRecordFilesStruct;

// compat with the old API that provided a tuple (md5,size,path,type)
static Py_ssize_t pkgSrcRecordFiles_length(PyObject *Self) {
   return 4;
}

// compat with the old API that provided a tuple (md5,size,path,type)
static PyObject* pkgSrcRecordFiles_item(PyObject *Self, Py_ssize_t i) {
   APT_IGNORE_DEPRECATED_PUSH
   PkgSrcRecordFilesStruct f = GetCpp<PkgSrcRecordFilesStruct>(Self);
   switch (i) {
      case 0:
         Py_INCREF(Py_None);
         return Py_None;
      case 1:
         return Py_BuildValue("N", MkPyNumber(f.FileSize));
      case 2:
         return Py_BuildValue("s", f.Path.c_str());
      case 3:
         return Py_BuildValue("s", f.Type.c_str());
   }

   PyErr_Format(PyExc_IndexError, "index %d out of range, consider using the properties instead", i);
   return NULL;
   APT_IGNORE_DEPRECATED_POP
}

static PySequenceMethods pkgsrcrecordfiles_as_sequence = {
   pkgSrcRecordFiles_length,0,0,pkgSrcRecordFiles_item,0,0,0,0,0,0
};

static PyObject *PkgSrcRecordFilesNew(PyTypeObject *type,PyObject *args,PyObject *kwds) {
   char *kwlist[] = {0};
   if (PyArg_ParseTupleAndKeywords(args,kwds,"",kwlist) == 0)
      return 0;

   return HandleErrors(CppPyObject_NEW<PkgSrcRecordFilesStruct>(NULL, type));
}

static const char *sourcerecordfile_doc =
    "SourceRecordFile()\n\n"
    "Provide an easy way to look up the src records of a source package.\n";

static PyObject *PkgSrcRecordFilesGetPath(PyObject *Self,void*) {
   PkgSrcRecordFilesStruct f = GetCpp<PkgSrcRecordFilesStruct>(Self);
   return CppPyString(f.Path.c_str());
}

static PyObject *PkgSrcRecordFilesGetType(PyObject *Self,void*) {
   PkgSrcRecordFilesStruct f = GetCpp<PkgSrcRecordFilesStruct>(Self);
   return CppPyString(f.Type.c_str());
}

static PyObject *PkgSrcRecordFilesGetSize(PyObject *Self,void*) {
   PkgSrcRecordFilesStruct f = GetCpp<PkgSrcRecordFilesStruct>(Self);
   return Py_BuildValue("N", MkPyNumber(f.FileSize));
}

static PyObject *PkgSrcRecordFilesGetHashes(PyObject *Self,void*) {
   PkgSrcRecordFilesStruct f = GetCpp<PkgSrcRecordFilesStruct>(Self);
   auto py = CppPyObject_NEW<HashStringList> (nullptr, &PyHashStringList_Type);
   py->Object = f.Hashes;
   return py;
}

static PyGetSetDef PkgSrcRecordFilesGetSet[] = {
   {"path",PkgSrcRecordFilesGetPath,0,
    "The remote path of the source package file."},
   {"type",PkgSrcRecordFilesGetType,0,
    "The type of the source package file."},
   {"size",PkgSrcRecordFilesGetSize,0,
    "The size of the source package file."},
   {"hashes",PkgSrcRecordFilesGetHashes,0,
    "The hashes of the source package file."},
   {}
};

PyTypeObject PySourceRecordFiles_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.SourceRecordFiles",     // tp_name
   sizeof(CppPyObject<PkgSrcRecordFilesStruct>),   // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<PkgSrcRecordFilesStruct>,   // tp_dealloc
   0,                                   // tp_print
   0,                                   // tp_getattr
   0,                                   // tp_setattr
   0,                                   // tp_compare
   0,                                   // tp_repr
   0,                                   // tp_as_number
   &pkgsrcrecordfiles_as_sequence,      // tp_as_sequence
   0,                                   // tp_as_mapping
   0,                                   // tp_hash
   0,                                   // tp_call
   0,                                   // tp_str
   _PyAptObject_getattro,               // tp_getattro
   0,                                   // tp_setattro
   0,                                   // tp_as_buffer
   (Py_TPFLAGS_DEFAULT |                // tp_flags
    Py_TPFLAGS_BASETYPE),
   sourcerecordfile_doc,                   // tp_doc
   0,                                   // tp_traverse
   0,                                   // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   0,                                   // tp_methods
   0,                                   // tp_members
   PkgSrcRecordFilesGetSet,             // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgSrcRecordFilesNew,                         // tp_new
};
// ---------------------------------------------------------------------

struct PkgSrcRecordsStruct
{
   pkgSourceList List;
   pkgSrcRecords *Records;
   pkgSrcRecords::Parser *Last;

   PkgSrcRecordsStruct() : Last(0) {
      List.ReadMainList();
      Records = new pkgSrcRecords(List);
   };
   ~PkgSrcRecordsStruct() {
      delete Records;
   };
};


// PkgSrcRecords Class							/*{{{*/
// ---------------------------------------------------------------------

static char *doc_PkgSrcRecordsLookup =
    "lookup(name: str) -> bool\n\n"
    "Look up the source package with the given name. Each call moves\n"
    "the position of the records parser forward. If there are no\n"
    "more records, return None. If the lookup failed this way,\n"
    "access to any of the attributes will result in an AttributeError.";
static PyObject *PkgSrcRecordsLookup(PyObject *Self,PyObject *Args)
{
   PkgSrcRecordsStruct &Struct = GetCpp<PkgSrcRecordsStruct>(Self);

   char *Name = 0;
   if (PyArg_ParseTuple(Args,"s",&Name) == 0)
      return 0;

   Struct.Last = Struct.Records->Find(Name, false);
   if (Struct.Last == 0) {
      Struct.Records->Restart();
      Py_INCREF(Py_None);
      return HandleErrors(Py_None);
   }

   return PyBool_FromLong(1);
}

static char *doc_PkgSrcRecordsRestart =
    "restart()\n\n"
    "Restart the lookup process. This moves the parser to the first\n"
    "package and lookups can now be made just like on a new object.";
static PyObject *PkgSrcRecordsRestart(PyObject *Self,PyObject *Args)
{
   PkgSrcRecordsStruct &Struct = GetCpp<PkgSrcRecordsStruct>(Self);

   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   Struct.Records->Restart();

   Py_INCREF(Py_None);
   return HandleErrors(Py_None);
}

static char *doc_PkgSrcRecordsStep =
    "step() -> bool\n\n"
    "Go to the source package. Each call moves\n"
    "the position of the records parser forward. If there are no\n"
    "more records, return None. If the lookup failed this way,\n"
    "access to any of the attributes will result in an AttributeError.";
static PyObject *PkgSrcRecordsStep(PyObject *Self,PyObject *Args)
{
   PkgSrcRecordsStruct &Struct = GetCpp<PkgSrcRecordsStruct>(Self);

   if (PyArg_ParseTuple(Args,"") == 0)
      return 0;

   Struct.Last = (pkgSrcRecords::Parser*)Struct.Records->Step();
   if (Struct.Last == 0) {
      Struct.Records->Restart();
      Py_INCREF(Py_None);
      return HandleErrors(Py_None);
   }

   return PyBool_FromLong(1);
}

static PyMethodDef PkgSrcRecordsMethods[] =
{
   {"lookup",PkgSrcRecordsLookup,METH_VARARGS,doc_PkgSrcRecordsLookup},
   {"restart",PkgSrcRecordsRestart,METH_VARARGS,doc_PkgSrcRecordsRestart},
   {"step",PkgSrcRecordsStep,METH_VARARGS,doc_PkgSrcRecordsStep},
   {}
};

/**
 * Get the PkgSrcRecordsStruct from a PyObject. If no package has been looked
 * up, set an AttributeError using the given name.
 */
static inline PkgSrcRecordsStruct &GetStruct(PyObject *Self,char *name) {
   PkgSrcRecordsStruct &Struct = GetCpp<PkgSrcRecordsStruct>(Self);
   if (Struct.Last == 0)
      PyErr_SetString(PyExc_AttributeError,name);
   return Struct;
}

static PyObject *PkgSrcRecordsGetPackage(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Package");
   return (Struct.Last != 0) ? CppPyString(Struct.Last->Package()) : 0;
}
static PyObject *PkgSrcRecordsGetVersion(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Version");
   return (Struct.Last != 0) ? CppPyString(Struct.Last->Version()) : 0;
}
static PyObject *PkgSrcRecordsGetMaintainer(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Maintainer");
   return (Struct.Last != 0) ? CppPyString(Struct.Last->Maintainer()) : 0;
}
static PyObject *PkgSrcRecordsGetSection(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Section");
   return (Struct.Last != 0) ? CppPyString(Struct.Last->Section()) : 0;
}
static PyObject *PkgSrcRecordsGetRecord(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Record");
   return (Struct.Last != 0) ? CppPyString(Struct.Last->AsStr()) : 0;
}
static PyObject *PkgSrcRecordsGetBinaries(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Binaries");
   if (Struct.Last == 0)
      return 0;
   PyObject *List = PyList_New(0);
   for(const char **b = Struct.Last->Binaries(); *b != 0; ++b)
      PyList_Append(List, CppPyString(*b));
   return List; // todo
}
static PyObject *PkgSrcRecordsGetIndex(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Index");
   if (Struct.Last == 0)
      return 0;
   const pkgIndexFile &tmp = Struct.Last->Index();
   CppPyObject<pkgIndexFile*> *PyObj;
   PyObj = CppPyObject_NEW<pkgIndexFile*>(Self,&PyIndexFile_Type,
                                              (pkgIndexFile*)&tmp);
   // Do not delete the pkgIndexFile*, it is managed by PkgSrcRecords::Parser.
   PyObj->NoDelete=true;
   return PyObj;
}

static PyObject *PkgSrcRecordsGetFiles(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"Files");
   if (Struct.Last == 0)
      return 0;
   PyObject *List = PyList_New(0);

   std::vector<pkgSrcRecords::File> f;
   if(!Struct.Last->Files(f))
      return NULL; // error

   PyObject *v;
   for(unsigned int i=0;i<f.size();i++) {
      v = CppPyObject_NEW<PkgSrcRecordFilesStruct>(Self, &PySourceRecordFiles_Type, f[i]);
      PyList_Append(List, v);
      Py_DECREF(v);
   }
   return List;
}

static PyObject *PkgSrcRecordsGetBuildDepends(PyObject *Self,void*) {
   PkgSrcRecordsStruct &Struct = GetStruct(Self,"BuildDepends");
   if (Struct.Last == 0)
      return 0;

   PyObject *Dict = PyDict_New();
   PyObject *Dep = 0;
   PyObject *LastDep = 0;
   PyObject *OrGroup = 0;
   
   std::vector<pkgSrcRecords::Parser::BuildDepRec> bd;
   if(!Struct.Last->BuildDepends(bd, false /* arch-only*/))
      return NULL; // error
   
   PyObject *v;
   for(unsigned int i=0;i<bd.size();i++) {
     
     Dep = CppPyString(pkgSrcRecords::Parser::BuildDepType(bd[i].Type));
     
	 LastDep = PyDict_GetItem(Dict,Dep);
	 if (LastDep == 0)
	 {
	    LastDep = PyList_New(0);
	    PyDict_SetItem(Dict,Dep,LastDep);
	    Py_DECREF(LastDep);
	 }
     Py_DECREF(Dep);
     OrGroup = PyList_New(0);
     PyList_Append(LastDep, OrGroup);
     Py_DECREF(OrGroup);

     // Add at least one package to the group, add more if Or is set.
     while (i < bd.size())
     {
	    v = Py_BuildValue("(sss)", bd[i].Package.c_str(),
			bd[i].Version.c_str(), pkgCache::CompType(bd[i].Op));
	    PyList_Append(OrGroup, v);
	    Py_DECREF(v);
	    if (pkgCache::Dep::Or != (bd[i].Op & pkgCache::Dep::Or))
	       break;
        i++;
     }
      
   }
   return Dict;
}


static PyGetSetDef PkgSrcRecordsGetSet[] = {
   {"binaries",PkgSrcRecordsGetBinaries,0,
    "A list of the names of the binaries produced by this source package."},
   {"build_depends",PkgSrcRecordsGetBuildDepends,0,
    "A dictionary describing the build-time dependencies of the package;\n"
    "the format is the same as used for apt_pkg.Version.depends_list_str."},
   {"files",PkgSrcRecordsGetFiles,0,
    "A list of :class:`SourceRecordFiles` objects."},
   {"index",PkgSrcRecordsGetIndex,0,
    "The index file associated with this record as a list of\n"
    "apt_pkg.IndexFile objects."},
   {"maintainer",PkgSrcRecordsGetMaintainer,0,
    "The maintainer of the package."},
   {"package",PkgSrcRecordsGetPackage,0,
    "The name of the source package."},
   {"record",PkgSrcRecordsGetRecord,0,
    "The raw record, suitable for parsing using apt_pkg.TagSection."},
   {"section",PkgSrcRecordsGetSection,0,
    "The section of the source package."},
   {"version",PkgSrcRecordsGetVersion,0,
    "The version of the source package."},
   {}
};

static PyObject *PkgSrcRecordsNew(PyTypeObject *type,PyObject *args,PyObject *kwds) {
   char *kwlist[] = {0};
   if (PyArg_ParseTupleAndKeywords(args,kwds,"",kwlist) == 0)
      return 0;

   return HandleErrors(CppPyObject_NEW<PkgSrcRecordsStruct>(NULL, type));
}

static const char *sourcerecords_doc =
    "SourceRecords()\n\n"
    "Provide an easy way to look up the records of source packages and\n"
    "provide easy attributes for some widely used fields of the record.";

PyTypeObject PySourceRecords_Type =
{
   PyVarObject_HEAD_INIT(&PyType_Type, 0)
   "apt_pkg.SourceRecords",     // tp_name
   sizeof(CppPyObject<PkgSrcRecordsStruct>),   // tp_basicsize
   0,                                   // tp_itemsize
   // Methods
   CppDealloc<PkgSrcRecordsStruct>,   // tp_dealloc
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
    Py_TPFLAGS_BASETYPE),
   sourcerecords_doc,                   // tp_doc
   0,                                   // tp_traverse
   0,                                   // tp_clear
   0,                                   // tp_richcompare
   0,                                   // tp_weaklistoffset
   0,                                   // tp_iter
   0,                                   // tp_iternext
   PkgSrcRecordsMethods,                   // tp_methods
   0,                                   // tp_members
   PkgSrcRecordsGetSet,                    // tp_getset
   0,                                   // tp_base
   0,                                   // tp_dict
   0,                                   // tp_descr_get
   0,                                   // tp_descr_set
   0,                                   // tp_dictoffset
   0,                                   // tp_init
   0,                                   // tp_alloc
   PkgSrcRecordsNew,                         // tp_new
};


									/*}}}*/

